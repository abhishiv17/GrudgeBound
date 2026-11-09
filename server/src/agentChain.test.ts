import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { createRng, createWorld, step, summarize, scriptedPolicy } from "@grudgebound/shared";
import { ChainWriter, type ChainOps, type LimitedKey } from "./chain";
import { ChainRules, displayName } from "./rules";
import { AgentChain, type AgentData, type AgentStore } from "./agentChain";

const CFG = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet", gameAccount: "gb.testnet" };
const GAME = "gb.testnet";
const FEN = "agent-fen.gb.testnet";

/** A fake chain: Ember, points, rules, limited accounts, and every write recorded. */
function fakeChain() {
  const calls: string[] = [];
  const ember: Record<string, number> = {};
  const points: Record<string, number> = {};
  const rules: { id: number; template_id: string; params: string; author: string; cost: number; status: string }[] = [];
  const battles = new Set<string>();
  const accounts = new Map<string, LimitedKey[]>();
  let submitFailures = 0;
  let views = 0;
  const ops: ChainOps = {
    async createLimitedAccount(account, keys) {
      if (accounts.has(account)) return;
      calls.push(`create ${account} ${keys.map((k) => `${k.contractId}:${k.methods.join("+")}`).join(" ")}`);
      accounts.set(account, keys);
    },
    async call(_c, method, args) {
      await new Promise((r) => setTimeout(r, 1));
      calls.push(`game ${method}`);
      if (method === "submit_battle") {
        if (submitFailures-- > 0) throw new Error("rpc timeout");
        const r = args.result as { battle_id: string; changes: { account_id: string; delta: number }[] };
        if (battles.has(r.battle_id)) throw new Error("battle already recorded");
        battles.add(r.battle_id);
        for (const c of r.changes) points[c.account_id] = Math.max(0, (points[c.account_id] ?? 0) + c.delta);
      }
      if (method === "mint_ember") ember[args.to as string] = (ember[args.to as string] ?? 0) + Number(args.amount);
      if (method === "resolve_rule") {
        const rule = rules[args.rule_id as number]!;
        rule.status = args.accepted ? "Active" : "Rejected";
        if (!args.accepted) points[rule.author] = (points[rule.author] ?? 0) + rule.cost;
      }
      return { hash: `h${calls.length}`, result: null };
    },
    async relay(who, contract, method, args, deposit) {
      await new Promise((r) => setTimeout(r, 1));
      calls.push(`${who} ${method}${deposit ? " +deposit" : ""}`);
      const keys = accounts.get(who);
      // A limited key can only call its own contract's listed methods, with no deposit.
      if (keys && (deposit || !keys.some((k) => k.contractId === contract && k.methods.includes(method)))) throw new Error("access key not allowed");
      if (method === "pay_minter") {
        const a = Number(args.amount);
        if ((ember[who] ?? 0) < a) throw new Error("insufficient balance");
        ember[who] = (ember[who] ?? 0) - a; ember[GAME] = (ember[GAME] ?? 0) + a;
      }
      if (method === "propose_rule") {
        const cost = 50 + 25 * rules.filter((r) => r.status === "Active").length;
        if ((points[who] ?? 0) < cost) throw new Error("not enough points");
        points[who] = (points[who] ?? 0) - cost;
        rules.push({ id: rules.length, template_id: args.template_id as string, params: args.params as string, author: who, cost, status: "Pending" });
        return { hash: `h${calls.length}`, result: rules.length - 1 };
      }
      return { hash: `h${calls.length}`, result: null };
    },
    async view(_c, method, args) {
      views++;
      if (method === "get_points") return points[args.account_id as string] ?? 0;
      if (method === "mt_balance_of") return String(ember[args.account_id as string] ?? 0);
      if (method === "rule_cost") return 50 + 25 * rules.filter((r) => r.status === "Active").length;
      if (method === "get_active_rules") return rules.filter((r) => r.status === "Active");
      throw new Error(method);
    },
  };
  return { calls, ember, points, rules, accounts, ops, failSubmits: (n: number) => { submitFailures = n; }, views: () => views };
}

function memoryStore(): AgentStore & { saves: number } {
  const s = { data: { version: 1, agents: [] } as AgentData, saves: 0, save() { s.saves++; } };
  return s;
}

const drain = async () => { for (let i = 0; i < 40; i++) await new Promise((r) => setTimeout(r, 3)); };

async function setup(limits = { dailyEmberCap: 200, actionsPerDay: 6 }) {
  const world = createWorld(2);
  const fake = fakeChain();
  const chain = new ChainWriter(CFG, fake.ops);
  const rules = new ChainRules(world, chain, (a) => a.endsWith(".gb.testnet"), { cooldownMs: 0, now: () => 0 });
  const store = memoryStore();
  const agents = new AgentChain(world, chain, rules, store, { gameAccount: GAME, network: "testnet", limits });
  await agents.provision();
  const fen = world.participants.find((p) => p.id === "fen")!;
  return { world, fake, chain, rules, store, agents, fen, hooks: agents.hooks() };
}

describe("agents' own accounts", () => {
  test("each agent gets one limited-key account; provisioning is idempotent and keys are per contract", async () => {
    const { fake, agents, store } = await setup();
    const creates = fake.calls.filter((c) => c.startsWith("create "));
    assert.equal(creates.length, 5);
    assert.ok(creates.includes(`create ${FEN} items.gb.testnet:pay_minter rite.gb.testnet:propose_rule`));
    assert.ok(store.data.agents.every((a) => a.ready));
    await agents.provision();
    assert.equal(fake.calls.filter((c) => c.startsWith("create ")).length, 5, "no second creation");
    const itemsKey = agents.keyFor(FEN, "items.gb.testnet");
    assert.ok(itemsKey && itemsKey !== agents.keyFor(FEN, "rite.gb.testnet"));
    assert.equal(agents.keyFor("someone.gb.testnet", "items.gb.testnet"), undefined);
    assert.equal(displayName(FEN), "Old Mother Fen");
  });

  test("refuses any network but testnet", () => {
    const world = createWorld(2);
    const chain = new ChainWriter({ ...CFG, network: "mainnet" }, fakeChain().ops);
    assert.throws(() => new AgentChain(world, chain, null, memoryStore(), { gameAccount: GAME, network: "mainnet" }), /testnet only/);
  });

  test("one batched submit_battle per tick with every agent's delta; failures are retried next tick", async () => {
    const { world, fake, agents, fen } = await setup();
    const vane = world.participants.find((p) => p.id === "vane")!;
    fen.points += 12; vane.points += 5;
    fake.failSubmits(1);
    agents.afterStep();
    await drain();
    assert.equal(fake.points[FEN] ?? 0, 0, "first write failed");
    world.tick += 1; fen.points += 3;
    agents.afterStep();
    await drain();
    assert.equal(fake.points[FEN], 15);
    assert.equal(fake.points["agent-vane.gb.testnet"], 5);
    const submits = fake.calls.filter((c) => c === "game submit_battle").length;
    agents.afterStep(); // nothing changed: no transaction
    await drain();
    assert.equal(fake.calls.filter((c) => c === "game submit_battle").length, submits);
  });

  test("a proposal goes through the agent's own account and joins the world only when resolved", async () => {
    const { world, fake, fen, hooks } = await setup();
    fake.points[FEN] = 100; fen.points = 100;
    const before = world.rules.length;
    const handled = hooks.onAgentProposal!(world, fen, "R1", { action: "pvp", ward: "lantern", multiplier: 2 });
    assert.equal(handled, true);
    assert.equal(world.rules.length, before, "not enacted in the sim directly");
    await drain();
    assert.ok(fake.calls.includes(`${FEN} propose_rule`));
    assert.ok(fake.calls.includes("game resolve_rule"));
    assert.equal(world.rules.length - before, 1, "enacted once resolved");
    assert.equal(world.rules.at(-1)!.author, "fen", "the rule credits the agent");
    assert.equal(fen.points, 100 - fake.rules[0]!.cost, "the sim pays the cost once");
  });

  test("an agent short of points on chain is turned away, never topped up", async () => {
    const { world, fake, fen, hooks } = await setup();
    fen.points = 500; // the sim thinks it is rich; the chain says otherwise
    hooks.onAgentProposal!(world, fen, "R1", { action: "pvp", ward: "lantern", multiplier: 2 });
    await drain();
    assert.ok(!fake.calls.includes(`${FEN} propose_rule`));
    assert.ok(world.events.some((e) => /turns Old Mother Fen away/.test(e.text)));
    assert.equal(fake.points[FEN] ?? 0, 0);
  });

  test("a buy is paid with pay_minter; a shortfall is minted up to the sim purse and the daily cap", async () => {
    const { world, fake, fen, hooks } = await setup({ dailyEmberCap: 30, actionsPerDay: 6 });
    fen.ember = 40;
    hooks.afterAction!(world, fen, { type: "buy", merchantId: "broker", itemId: "blank-talisman" }, 60); // paid 20 in the sim
    await drain();
    assert.ok(fake.calls.includes("game mint_ember"));
    assert.ok(fake.calls.includes(`${FEN} pay_minter`));
    assert.ok(!fake.calls.some((c) => c.includes("+deposit")), "no deposit under a limited key");
    assert.equal(fake.ember[FEN], 0);
    assert.equal(fake.ember[GAME], 20);
    // 20 of today's 30 minted: a second 20-Ember buy would need 20 more, so it stays in the sim.
    fen.ember = 20;
    hooks.afterAction!(world, fen, { type: "buy", merchantId: "broker", itemId: "blank-talisman" }, 40);
    await drain();
    assert.equal(fake.calls.filter((c) => c === `${FEN} pay_minter`).length, 1);
    assert.equal(fake.ember[GAME], 20);
  });

  test("at most N agent-signed actions per in-game day, then sim-only", async () => {
    const { world, fake, fen, hooks } = await setup({ dailyEmberCap: 1000, actionsPerDay: 2 });
    fake.ember[FEN] = 1000;
    for (let i = 0; i < 4; i++) hooks.afterAction!(world, fen, { type: "buy", merchantId: "broker", itemId: "ink-salve" }, 100);
    await drain();
    assert.equal(fake.calls.filter((c) => c === `${FEN} pay_minter`).length, 2);
    assert.equal(hooks.onAgentProposal!(world, fen, "R1", { action: "pvp", ward: "lantern", multiplier: 2 }), false, "limit reached: the sim handles it");
  });

  test("GET /agents answers are shared for a few seconds and never contain keys", async () => {
    const { agents, fake } = await setup();
    const base = fake.views();
    const first = await agents.view(5000, 1_000);
    const perAnswer = fake.views() - base;
    assert.ok(perAnswer > 0);
    await agents.view(5000, 3_000);
    assert.equal(fake.views() - base, perAnswer, "a second request within 5 s reuses the answer");
    await agents.view(5000, 7_000);
    assert.equal(fake.views() - base, perAnswer * 2, "a later request reads the chain again");
    assert.equal(first.length, 5);
    assert.ok(!JSON.stringify(first).includes("ed25519:"), "no key material in the view");
  });

  test("without hooks (AGENT_CHAIN=off) the simulation is unchanged", () => {
    const a = createWorld(2), b = createWorld(2);
    const ra = createRng(2), rb = createRng(2);
    for (let i = 0; i < 24 * 5; i++) { step(a, ra); step(b, rb, scriptedPolicy, {}); }
    assert.deepEqual(summarize(a), summarize(b));
  });
});

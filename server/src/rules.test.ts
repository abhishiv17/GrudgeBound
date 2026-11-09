import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { createWorld } from "@grudgebound/shared";
import { ChainWriter, type ChainOps } from "./chain";
import { ChainRules, displayName, rulesOverview, templateCatalog, type ProposeAnswer, type RepealAnswer } from "./rules";

const CFG = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet" };
const AFREEN = "afreen.gb.testnet";

/** A fake rite contract: points per account, rules, and every call recorded. */
function fakeRite(points: Record<string, number> = { [AFREEN]: 120 }) {
  const calls: string[] = [];
  /** resolve_rule failures to inject: "before" = refused, "after" = applied but the answer was lost. */
  const resolveFailures: ("before" | "after")[] = [];
  const rules: { id: number; template_id: string; params: string; author: string; cost: number; status: string }[] = [];
  const cost = () => 50 + 25 * rules.filter((r) => r.status === "Active").length;
  const ops: ChainOps = {
    async call(_c, method, args) {
      await new Promise((r) => setTimeout(r, 2));
      calls.push(`game ${method}`);
      if (method === "resolve_rule") {
        const failure = resolveFailures.shift();
        if (failure === "before") throw new Error("rpc timeout");
        const rule = rules[args.rule_id as number]!;
        if (rule.status !== "Pending") throw new Error("rule is not pending");
        rule.status = args.accepted ? "Active" : "Rejected";
        if (!args.accepted) points[rule.author] = (points[rule.author] ?? 0) + rule.cost;
        if (failure === "after") throw new Error("rpc timeout");
      }
      return { hash: `h${calls.length}`, result: null };
    },
    async relay(player, _c, method, args) {
      await new Promise((r) => setTimeout(r, 2));
      calls.push(`${player} ${method}`);
      if (method === "repeal_rule") {
        const rule = rules[args.rule_id as number];
        if (!rule || rule.status !== "Active") throw new Error("rule is not in force");
        if ((points[player] ?? 0) < rule.cost * 2) throw new Error("not enough points");
        points[player] = (points[player] ?? 0) - rule.cost * 2;
        rule.status = "Repealed";
        return { hash: `h${calls.length}`, result: null };
      }
      const c = cost();
      points[player] = (points[player] ?? 0) - c;
      rules.push({ id: rules.length, template_id: args.template_id as string, params: args.params as string, author: player, cost: c, status: "Pending" });
      return { hash: `h${calls.length}`, result: rules.length - 1 };
    },
    async view(_c, method, args) {
      if (method === "get_points") return points[args.account_id as string] ?? 0;
      if (method === "rule_cost") return cost();
      if (method === "get_active_rules") return rules.filter((r) => r.status === "Active");
      if (method === "get_rules") return rules.slice(args.from_index as number, (args.from_index as number) + (args.limit as number));
      throw new Error(method);
    },
  };
  return { calls, rules, points, ops, resolveFailures };
}

function setup(points?: Record<string, number>) {
  const world = createWorld(7);
  const rite = fakeRite(points);
  const chain = new ChainWriter(CFG, rite.ops);
  let time = 0;
  const rules = new ChainRules(world, chain, (a) => a.endsWith(".gb.testnet"), { cooldownMs: 60_000, now: () => time });
  return { world, rite, chain, rules, advance: () => { time += 60_000; } };
}

const settle = () => new Promise((r) => setTimeout(r, 40));
const PVP_BAN = { templateId: "R8", params: { ward: "lantern" } };
const PVP_BONUS = { templateId: "R1", params: { action: "pvp", ward: "lantern", multiplier: 2 } };

describe("rules through the rite contract", () => {
  test("one pending proposal per account, then a cooldown, including concurrent checks", async () => {
    const { rules, advance } = setup({ [AFREEN]: 500 });
    const first = rules.propose({ account: AFREEN, ...PVP_BAN });
    assert.equal((await rules.propose({ account: AFREEN, ...PVP_BONUS })).status, 409);
    await first;
    assert.equal((await rules.propose({ account: AFREEN, ...PVP_BONUS })).status, 409);
    await settle();
    assert.equal((await rules.propose({ account: AFREEN, ...PVP_BONUS })).status, 429);
    advance();
    assert.equal((await rules.propose({ account: AFREEN, ...PVP_BONUS })).status, 200);
    await settle();
  });

  test("concurrent contradictory proposals reserve accepted rules before resolution", async () => {
    const bob = "bob.gb.testnet";
    const { world, rules, rite } = setup({ [AFREEN]: 500, [bob]: 500 });
    const results = await Promise.all([rules.propose({ account: AFREEN, ...PVP_BAN }), rules.propose({ account: bob, ...PVP_BONUS })]);
    assert.deepEqual(results.map((r) => (r.body as ProposeAnswer).accepted), [true, false]);
    assert.equal(world.rules.length, 0);
    await settle();
    assert.equal(world.rules.length, 1);
    assert.deepEqual(rite.rules.map((r) => r.status), ["Active", "Rejected"]);
  });
  test("a failed verdict is settled once more, and a verdict whose answer was lost is not sent twice", async () => {
    for (const failure of ["before", "after"] as const) {
      const { world, rite, chain, rules, advance } = setup({ [AFREEN]: 500 });
      rite.resolveFailures.push(failure);
      const r = await rules.propose({ account: AFREEN, ...PVP_BAN });
      await settle();
      const receipt = chain.ruleReceipt((r.body as ProposeAnswer).jobId)!;
      assert.equal(receipt.status, "done", failure);
      assert.equal(rite.rules[0]!.status, "Active");
      assert.equal(world.rules.length, 1, "enacted exactly once");
      assert.equal(rite.calls.filter((c) => c === "game resolve_rule").length, failure === "before" ? 2 : 1);
      advance();
      assert.equal((await rules.propose({ account: AFREEN, ...PVP_BONUS })).status, 200, "the proposal slot is free again");
      await settle();
    }
  });

  test("when the second try fails too, the proposal stays reserved (its verdict is uncertain)", async () => {
    const { rite, chain, rules, advance } = setup({ [AFREEN]: 500 });
    rite.resolveFailures.push("before", "before");
    const r = await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
    assert.equal(chain.ruleReceipt((r.body as ProposeAnswer).jobId)!.status, "failed");
    advance();
    assert.equal((await rules.propose({ account: AFREEN, ...PVP_BONUS })).status, 409);
  });

  test("an accepted rule is relayed for the player, resolved by the game master, then in force in the world", async () => {
    const { world, rite, chain, rules } = setup();
    const r = await rules.propose({ account: AFREEN, ...PVP_BAN });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    const a = r.body as ProposeAnswer;
    assert.equal(a.accepted, true);
    assert.equal(a.cost, 50);
    assert.equal(a.chain.status, "pending");
    assert.equal(world.rules.length, 0, "not in force before the chain says so");
    await settle();
    assert.deepEqual(rite.calls, [`${AFREEN} propose_rule`, "game resolve_rule"]);
    const receipt = chain.ruleReceipt(a.jobId)!;
    assert.equal(receipt.status, "done");
    assert.equal(receipt.ruleId, 0);
    assert.deepEqual(receipt.txs.map((t) => t.kind), ["propose", "resolve"]);
    assert.equal(world.rules.length, 1);
    assert.equal(world.rules[0]!.author, AFREEN);
    assert.match(world.events.at(-1)!.text, /Afreen spent 50 points/);
    assert.equal(rite.points[AFREEN], 70);
  });

  test("a contradiction is rejected on chain with Kessa's reason, refunded, and stays out of the world", async () => {
    const { world, rite, rules, advance } = setup({ [AFREEN]: 200 });
    await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
    advance();
    const r = await rules.propose({ account: AFREEN, ...PVP_BONUS });
    const a = r.body as ProposeAnswer;
    assert.equal(a.accepted, false);
    assert.match(a.reason, /Contradicts "No PvP in Lantern Ward"/);
    await settle();
    assert.equal(world.rules.length, 1);
    assert.equal(rite.rules[1]!.status, "Rejected");
    assert.equal(rite.points[AFREEN], 150, "only the accepted rule's 50 points are spent");
    assert.match(world.events.at(-1)!.text, /Kessa rejects Afreen's rule .*refunded/);
  });

  test("refusals that never reach the chain", async () => {
    const { rite, rules } = setup({ [AFREEN]: 10 });
    const poor = await rules.propose({ account: AFREEN, ...PVP_BAN });
    assert.equal(poor.status, 402);
    assert.match((poor.body as { error: string }).error, /costs 50 points; Afreen has 10/);
    assert.equal((await rules.propose({ account: "bob.testnet", ...PVP_BAN })).status, 403);
    assert.equal((await rules.propose({ account: "Not An Account", ...PVP_BAN })).status, 400);
    assert.equal((await rules.propose({ account: AFREEN, templateId: "R99", params: {} })).status, 400);
    assert.equal((await rules.propose({ account: AFREEN, templateId: "R8", params: { ward: "atlantis" } })).status, 400);
    assert.equal((await rules.propose({ account: AFREEN, templateId: "R8", params: [] })).status, 400);
    assert.deepEqual(rite.calls, []);
  });

  test("on start, rules in force on chain are loaded once; unreadable ones are skipped", async () => {
    const { world, rite, chain } = setup();
    rite.rules.push(
      { id: 0, template_id: "R8", params: JSON.stringify({ ward: "lantern" }), author: AFREEN, cost: 50, status: "Active" },
      { id: 1, template_id: "R99", params: "{}", author: AFREEN, cost: 75, status: "Active" },
      { id: 2, template_id: "R8", params: "not json", author: AFREEN, cost: 75, status: "Active" },
    );
    const rules = new ChainRules(world, chain, () => true);
    assert.equal(await rules.syncFromChain(), 1);
    assert.equal(await rules.syncFromChain(), 0, "a second sync adds nothing");
    assert.equal(world.rules.length, 1);
  });

  test("a repeal is relayed for the player, costs twice the rule, and takes the rule out of the world", async () => {
    const { world, rite, rules } = setup({ [AFREEN]: 200 });
    await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
    assert.equal(rite.points[AFREEN], 150);
    const o = await rulesOverview(world, null, AFREEN, rules);
    assert.deepEqual(o.active.map((r) => [r.chainId, r.repealCost]), [[0, 100]]);

    const r = await rules.repeal({ account: AFREEN, ruleId: "0" });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    const a = r.body as RepealAnswer;
    assert.equal(a.cost, 100);
    assert.equal(a.text, "No PvP in Lantern Ward");
    assert.equal((await rules.repeal({ account: AFREEN, ruleId: 0 })).status, 409, "no second repeal while one is on its way");
    await settle();
    assert.equal(world.rules.length, 0);
    assert.equal(rite.rules[0]!.status, "Repealed");
    assert.equal(rite.points[AFREEN], 50);
    assert.match(world.events.at(-1)!.text, /Afreen paid 100 points\. Rule #1 is struck/);
    assert.deepEqual(rite.calls.slice(-1), [`${AFREEN} repeal_rule`]);
    assert.equal((await rules.repeal({ account: AFREEN, ruleId: 0 })).status, 404, "already gone");
  });

  test("repeal refusals, and a failed repeal leaves the rule in force", async () => {
    const { world, rite, chain, rules } = setup({ [AFREEN]: 120 });
    await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
    const poor = await rules.repeal({ account: AFREEN, ruleId: 0 });
    assert.equal(poor.status, 402);
    assert.match((poor.body as { error: string }).error, /costs 100 points; Afreen has 70/);
    assert.equal((await rules.repeal({ account: "bob.testnet", ruleId: 0 })).status, 403);
    assert.equal((await rules.repeal({ account: AFREEN, ruleId: -1 })).status, 400);
    assert.equal((await rules.repeal({ account: AFREEN, ruleId: 9 })).status, 404);

    rite.points[AFREEN] = 500;
    rite.rules[0]!.status = "Rejected"; // the contract will refuse
    const r = await rules.repeal({ account: AFREEN, ruleId: 0 });
    await settle();
    assert.equal(chain.ruleReceipt((r.body as RepealAnswer).jobId)!.status, "failed");
    assert.equal(world.rules.length, 1, "still in force in the world");
    rite.rules[0]!.status = "Active";
    assert.equal((await rules.repeal({ account: AFREEN, ruleId: 0 })).status, 200, "may try again");
  });

  test("a rule repealed on chain by someone else leaves the world on the next sync", async () => {
    const { world, rite, rules } = setup();
    await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
    rite.rules[0]!.status = "Repealed";
    await rules.syncFromChain();
    assert.equal(world.rules.length, 0);
    assert.match(world.events.at(-1)!.text, /struck from the Rite/);
  });

  test("number parameters sent as text are stored as numbers", async () => {
    const { world, rules } = setup();
    const r = await rules.propose({ account: AFREEN, templateId: "R4", params: { amount: "5" } });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    await settle();
    assert.deepEqual(world.rules[0]!.params, { amount: 5 });
  });

  test("display names", () => {
    assert.equal(displayName("akshay.grudgebound.testnet"), "Akshay");
  });

  test("the Rules panel's catalog spells every value the way the rule reads", () => {
    const cat = templateCatalog();
    assert.equal(cat.length, 8, "the 8 MUST templates");
    const r1 = cat.find((t) => t.id === "R1")!;
    assert.deepEqual(r1.params.map((p) => p.key), ["action", "ward", "multiplier"]);
    assert.deepEqual(r1.params[0]!.options.find((o) => o.value === "pvp"), { value: "pvp", label: "PvP" });
    assert.deepEqual(r1.params[1]!.options[0], { value: "lantern", label: "Lantern Ward" });
    const r4 = cat.find((t) => t.id === "R4")!;
    assert.equal(r4.params[0]!.options.length, 20, "a 1-20 range becomes 20 values");
  });

  test("overview reads cost and points from the chain, and still answers when the chain is down", async () => {
    const { world, chain, rules } = setup();
    await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
    const o = await rulesOverview(world, chain, AFREEN);
    assert.equal(o.chain, true);
    assert.equal(o.cost, 75);
    assert.equal(o.points, 70);
    assert.deepEqual(o.active.map((r) => r.text), ["No PvP in Lantern Ward"]);
    const down = new ChainWriter(CFG, { call: rite0, relay: rite0, view: async () => { throw new Error("rpc down"); } });
    const d = await rulesOverview(world, down, AFREEN);
    assert.equal(d.points, null);
    const off = await rulesOverview(world, null, AFREEN);
    assert.equal(off.chain, false);
  });
});

const rite0 = async (): Promise<never> => { throw new Error("unused"); };

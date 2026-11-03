import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { createWorld } from "@grudgebound/shared";
import { ChainWriter, type ChainOps } from "./chain";
import { ChainRules, displayName, type ProposeAnswer } from "./rules";

const CFG = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet" };
const AFREEN = "afreen.gb.testnet";

/** A fake rite contract: points per account, rules, and every call recorded. */
function fakeRite(points: Record<string, number> = { [AFREEN]: 120 }) {
  const calls: string[] = [];
  const rules: { id: number; template_id: string; params: string; author: string; cost: number; status: string }[] = [];
  const cost = () => 50 + 25 * rules.filter((r) => r.status === "Active").length;
  const ops: ChainOps = {
    async call(_c, method, args) {
      await new Promise((r) => setTimeout(r, 2));
      calls.push(`game ${method}`);
      if (method === "resolve_rule") {
        const rule = rules[args.rule_id as number]!;
        rule.status = args.accepted ? "Active" : "Rejected";
        if (!args.accepted) points[rule.author] = (points[rule.author] ?? 0) + rule.cost;
      }
      return { hash: `h${calls.length}`, result: null };
    },
    async relay(player, _c, method, args) {
      await new Promise((r) => setTimeout(r, 2));
      calls.push(`${player} ${method}`);
      const c = cost();
      points[player] = (points[player] ?? 0) - c;
      rules.push({ id: rules.length, template_id: args.template_id as string, params: args.params as string, author: player, cost: c, status: "Pending" });
      return { hash: `h${calls.length}`, result: rules.length - 1 };
    },
    async view(_c, method, args) {
      if (method === "get_points") return points[args.account_id as string] ?? 0;
      if (method === "rule_cost") return cost();
      if (method === "get_active_rules") return rules.filter((r) => r.status === "Active");
      throw new Error(method);
    },
  };
  return { calls, rules, points, ops };
}

function setup(points?: Record<string, number>) {
  const world = createWorld(7);
  const rite = fakeRite(points);
  const chain = new ChainWriter(CFG, rite.ops);
  const rules = new ChainRules(world, chain, (a) => a.endsWith(".gb.testnet"));
  return { world, rite, chain, rules };
}

const settle = () => new Promise((r) => setTimeout(r, 40));
const PVP_BAN = { templateId: "R8", params: { ward: "lantern" } };
const PVP_BONUS = { templateId: "R1", params: { action: "pvp", ward: "lantern", multiplier: 2 } };

describe("rules through the rite contract", () => {
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
    const { world, rite, rules } = setup({ [AFREEN]: 200 });
    await rules.propose({ account: AFREEN, ...PVP_BAN });
    await settle();
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

  test("display names", () => {
    assert.equal(displayName("akshay.grudgebound.testnet"), "Akshay");
  });
});

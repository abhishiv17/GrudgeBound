import { describe, test } from "node:test";
import assert from "node:assert/strict";
import { createWorld } from "@grudgebound/shared";
import { AuthError } from "./auth";
import { BattleSessions, type StartAnswer, type TurnAnswer } from "./battles";
import { ChainWriter, type ChainVow } from "./chain";
import { Vows } from "./vows";

const account = "binder.game.testnet";
const error = (status: number) => (e: unknown) => e instanceof AuthError && e.status === status;
async function until(done: () => boolean) {
  for (let i = 0; i < 100 && !done(); i++) await new Promise<void>((r) => setImmediate(r));
  assert.ok(done(), "chain job should settle");
}
function setup() {
  let now = 100_000, points = 100, fail = false, failures = 0;
  const stored: ChainVow[] = [];
  const calls: { method: string; args: Record<string, unknown> }[] = [];
  const world = createWorld(2);
  const chain = new ChainWriter({ network: "testnet", itemsContract: "items.game.testnet", riteContract: "rite.game.testnet", guestAccount: account }, {
    view: async (_contract, method) => {
      if (method === "get_vows") return structuredClone(stored);
      if (method === "get_points") return points;
      if (method === "talismans_of") return { talismans: [{ token_id: "tal-1", owner: account, meta: { species: "cinder-grin" } }], next_index: null };
      throw new Error(`Unexpected view ${method}`);
    },
    relay: async (who, _contract, method, args) => {
      assert.equal(who, account); assert.equal(method, "make_vow");
      if (fail) { failures++; throw new Error("relay unavailable"); }
      calls.push({ method, args }); points -= args.stake as number;
      const id = stored.length;
      stored.push({ id, owner: who, template_id: args.template_id as string, params: args.params as string,
        stake: args.stake as number, created_ms: now, ends_ms: now + (args.duration_ms as number), status: "Active" });
      return { hash: "swear-proof", result: id };
    },
    call: async (_contract: string, method: string, args: Record<string, unknown>) => {
      assert.equal(method, "settle_vow");
      if (fail) { failures++; throw new Error("settlement unavailable"); }
      const vow = stored[args.vow_id as number]!; assert.equal(vow.status, "Active");
      if (args.kept) { assert.ok(now >= vow.ends_ms); points += vow.stake + Math.floor(vow.stake * 20 / 100); }
      vow.status = args.kept ? "Kept" : "Broken"; calls.push({ method, args });
      return { hash: "settle-proof", result: null };
    },
  });
  const vows = new Vows(world, chain, (a) => a === account, 1000, () => now);
  async function swear(templateId = "V2", params = {}, days = 1) {
    const result = await vows.swear(account, { templateId, params, days });
    await until(() => chain.vowReceipt(result.jobId)?.status !== "pending");
    return chain.vowReceipt(result.jobId)!;
  }
  return { world, vows, chain, calls, stored, swear, advance: (ms: number) => { now += ms; }, setFail: (v: boolean) => { fail = v; }, now: () => now, failures: () => failures };
}

describe("Unreal Vow Shrine stakes and restrictions", () => {
  test("relays the fixed stake for the authenticated player and uses town days, never client stake/deadline values", async () => {
    const s = setup();
    const result = await s.vows.swear(account, { templateId: "V1", params: { ward: "lantern", arbitrary: true }, days: 2, stake: 0, endsAt: 0 });
    await until(() => s.chain.vowReceipt(result.jobId)?.status !== "pending");
    const receipt = s.chain.vowReceipt(result.jobId)!;
    assert.equal(receipt.status, "done"); assert.equal(receipt.vowId, 0);
    assert.equal(receipt.txs[0]!.url, "https://testnet.nearblocks.io/txns/swear-proof");
    assert.deepEqual(s.calls[0]!.args, { template_id: "V1", params: '{"ward":"lantern"}', stake: 20, duration_ms: 48_000 });
    const view = await s.vows.overview(account);
    assert.equal(view.points, 80); assert.equal(view.daySeconds, 24);
    assert.equal(view.vows[0]!.text, "I will not leave Lantern Ward"); assert.equal(view.vows[0]!.payout, 24);
    assert.deepEqual(view.templates.map((t) => t.id), ["V1", "V2", "V3", "V4"]);
  });
  test("rejects malformed vows, wrong durations, foreign talismans, unsupported templates and duplicate active vows", async () => {
    const s = setup();
    await assert.rejects(s.vows.swear("other.game.testnet", { templateId: "V2", params: {}, days: 1 }), error(503));
    await assert.rejects(s.vows.swear(account, { templateId: "V6", params: {}, days: 1 }), error(400));
    await assert.rejects(s.vows.swear(account, { templateId: "V1", params: { ward: "unknown" }, days: 1 }), error(400));
    await assert.rejects(s.vows.swear(account, { templateId: "V2", params: {}, days: 0 }), error(400));
    await assert.rejects(s.vows.swear(account, { templateId: "V2", params: {}, days: 1.5 }), error(400));
    await assert.rejects(s.vows.swear(account, { templateId: "V3", params: { talisman: "somebody-elses" }, days: 3 }), error(403));
    await assert.rejects(s.vows.swear(account, { templateId: "V4", params: { omen: "haunt" }, days: 1 }), error(400));
    await s.swear(); await assert.rejects(s.vows.swear(account, { templateId: "V2", params: {}, days: 1 }), error(409));
    assert.equal(s.calls.length, 1);
  });
  test("the stake returns with 20% only after the contract deadline, including after server restart", async () => {
    const s = setup(); await s.swear();
    await s.vows.sweep([account]); assert.equal(s.stored[0]!.status, "Active");
    s.advance(24_000);
    const restarted = new Vows(s.world, s.chain, () => true, 1000, s.now);
    assert.equal((await restarted.overview(account)).vows[0]!.status, "Settling");
    await restarted.sweep([account, account]); await until(() => s.stored[0]!.status === "Kept");
    assert.equal((await restarted.overview(account)).points, 103);
    await restarted.sweep([account]); assert.equal(s.calls.filter((c) => c.method === "settle_vow").length, 1);
  });
  test("an early release burns only its owner's stake and adds dread only after successful settlement", async () => {
    const s = setup(); await s.swear();
    const dread = s.world.wards.lantern.grudge.dread;
    await assert.rejects(s.vows.release("somebody-else.testnet", 0), error(404));
    const result = await s.vows.release(account, 0);
    await until(() => s.chain.vowReceipt(result.jobId)?.status !== "pending");
    assert.equal(s.stored[0]!.status, "Broken"); assert.equal((await s.vows.overview(account)).points, 85);
    assert.equal(s.world.wards.lantern.grudge.dread, dread + 4);
    await assert.rejects(s.vows.release(account, 0), error(409));
  });
  test("failed writes unlock retries and expired vows cannot be burned by a client", async () => {
    const s = setup(); s.setFail(true);
    assert.equal((await s.swear()).status, "failed"); assert.equal((await s.vows.overview(account)).points, 100);
    s.setFail(false); await s.swear(); s.advance(24_000);
    await assert.rejects(s.vows.release(account, 0), error(409));
    s.setFail(true); await s.vows.sweep([account]);
    await until(() => s.failures() === 2);
    await new Promise<void>((r) => setImmediate(r));
    assert.equal(s.stored[0]!.status, "Active");
    s.setFail(false); await s.vows.sweep([account]); await until(() => s.stored[0]!.status === "Kept");
  });
  test("an account cannot race two swear requests", async () => {
    const s = setup();
    const first = s.vows.swear(account, { templateId: "V2", params: {}, days: 1 });
    await assert.rejects(s.vows.swear(account, { templateId: "V1", params: { ward: "lantern" }, days: 1 }), error(409));
    const result = await first; await until(() => s.chain.vowReceipt(result.jobId)?.status !== "pending");
    assert.equal(s.stored.length, 1);
  });
  test("promised restrictions reject moves before a turn is spent; expiry releases them", async () => {
    const s = setup(); await s.swear("V1", { ward: "lantern" }); await s.swear("V2");
    const snapshot = await s.vows.snapshot(account);
    const guard = (c: Parameters<Vows["battleGuard"]>[1], a?: Parameters<Vows["battleGuard"]>[2]) => s.vows.battleGuard(snapshot, c, a);
    const battles = new BattleSessions(s.world, { newSeed: () => 1 });
    assert.equal(battles.start({ ward: "tide", account }, guard).status, 409);
    const started = battles.start({ ward: "lantern", account }, guard); assert.equal(started.status, 200);
    assert.equal((started.body as StartAnswer).view.canUse.salve, false);
    assert.ok((started.body as StartAnswer).lines.some((line) => line.includes("vow forbids healing")));
    const battleId = (started.body as StartAnswer).battleId;
    assert.equal(battles.turn({ battleId, action: { kind: "item", item: "salve" } }, account, guard).status, 409);
    const turn = battles.turn({ battleId, action: { kind: "ward" } }, account, guard);
    assert.equal(turn.status, 200); assert.equal((turn.body as TurnAnswer).view.turn, (started.body as StartAnswer).view.turn + 1);
    assert.throws(() => s.vows.assertTrade(snapshot, "tide"), error(409));
    s.advance(24_000); assert.equal(guard({ ward: "tide", omen: "whisper" }, { kind: "attack" }), null);
    battles.abandonAccount(account); assert.equal(battles.turn({ battleId, action: { kind: "attack" } }, account).status, 404);
  });
  test("a promised talisman cannot be sold; an omen vow still allows fleeing", async () => {
    const s = setup(); await s.swear("V3", { talisman: "tal-1" }, 3); await s.swear("V4", { omen: "wail" });
    const snapshot = await s.vows.snapshot(account);
    assert.throws(() => s.vows.assertTrade(snapshot, "lantern", "tal-1"), error(409));
    s.vows.assertTrade(snapshot, "lantern", "tal-2");
    assert.ok(s.vows.battleGuard(snapshot, { ward: "lantern", omen: "whisper" }, { kind: "attack" }));
    assert.equal(s.vows.battleGuard(snapshot, { ward: "lantern", omen: "whisper" }, { kind: "flee" }), null);
  });
});

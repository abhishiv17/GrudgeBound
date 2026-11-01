import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { capturePoints, createWorld, replayBattle, DEFAULT_PLAYER_CURSE } from "@grudgebound/shared";
import { BattleSessions, BATTLE_IDLE_MS, MAX_OPEN_BATTLES, parseAction, type StartAnswer, type TurnAnswer } from "./battles";

function setup(seed = 1234) {
  let clock = 1_000_000;
  let ids = 0;
  const world = createWorld(7);
  const sessions = new BattleSessions(world, { now: () => clock, newSeed: () => seed, newId: () => `b${++ids}` });
  return { world, sessions, tick: (ms: number) => (clock += ms) };
}

function start(sessions: BattleSessions, body: unknown = { ward: "lantern", by: "Tester" }): StartAnswer {
  const r = sessions.start(body);
  assert.equal(r.status, 200, JSON.stringify(r.body));
  return r.body as StartAnswer;
}

/** Attacks until the curse is weak, then seals; returns the last answer. */
function fightToTheEnd(sessions: BattleSessions, battleId: string): TurnAnswer {
  for (let i = 0; i < 200; i++) {
    const r = sessions.turn({ battleId, action: { kind: "attack" } });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    const a = r.body as TurnAnswer;
    if (a.outcome) return a;
  }
  throw new Error("battle never ended");
}

describe("server-run battles (P1-C5)", () => {
  test("start picks a curse from the Ward, shows the battle and keeps the seed secret", () => {
    const { sessions } = setup();
    const a = start(sessions);
    assert.equal(a.battleId, "b1");
    assert.equal(a.ward, "lantern");
    assert.equal(a.view.wild.speciesId, a.wild.id);
    assert.equal(a.view.player.speciesId, DEFAULT_PLAYER_CURSE);
    assert.equal(a.view.turn, 0);
    assert.equal(a.view.outcome, null);
    assert.ok(a.view.chances.seal >= 0 && a.view.chances.seal < 1, "a fresh curse is hard to seal");
    assert.ok(a.view.chances.flee > 0);
    assert.equal(a.lines.length, 1);
    assert.ok(!("seed" in a), "the seed must not be sent before the battle ends");
    assert.equal(sessions.openCount, 1);
  });

  test("bad starts are refused", () => {
    const { sessions } = setup();
    assert.equal(sessions.start({ ward: "nowhere" }).status, 400);
    assert.equal(sessions.start({ ward: "lantern", playerCurse: "not-a-curse" }).status, 400);
    assert.equal(sessions.start(null).status, 400);
  });

  test("a battle fought turn by turn ends, pays points and replays from the seed it reveals", () => {
    const { sessions, world } = setup();
    const a = start(sessions);
    const end = fightToTheEnd(sessions, a.battleId);
    assert.ok(end.outcome === "won" || end.outcome === "lost", `attacks only, got ${end.outcome}`);
    assert.equal(end.seed, 1234);
    assert.ok(end.actions && end.actions.length === end.view.turn);
    // anyone can check it
    const replay = replayBattle({ wild: a.wild.id, omen: a.wild.omen, playerCurse: DEFAULT_PLAYER_CURSE }, end.seed!, end.actions!);
    assert.equal(replay.at(-1)!.state.outcome, end.outcome);
    if (end.outcome === "won") {
      assert.equal(end.points, capturePoints(world.rules, "exorcise", "lantern", a.wild.omen));
      assert.ok(world.events.some((e) => e.text.includes("Tester")), "the town log names the player");
    } else assert.equal(end.points, 0);
    // the battle is closed
    assert.equal(sessions.turn({ battleId: a.battleId, action: { kind: "attack" } }).status, 404);
    assert.equal(sessions.openCount, 0);
  });

  test("the same seed gives the same battle", () => {
    const one = setup(99);
    const two = setup(99);
    const a = start(one.sessions);
    const b = start(two.sessions);
    assert.deepEqual(a.wild, b.wild);
    assert.deepEqual(fightToTheEnd(one.sessions, a.battleId).lines, fightToTheEnd(two.sessions, b.battleId).lines);
  });

  test("each turn returns events with matching log lines", () => {
    const { sessions } = setup();
    const a = start(sessions);
    const r = sessions.turn({ battleId: a.battleId, action: { kind: "ward" } });
    assert.equal(r.status, 200);
    const t = r.body as TurnAnswer;
    assert.equal(t.action, "ward");
    assert.equal(t.events.length, t.lines.length);
    assert.ok(t.lines.some((l) => l.includes("paper ward")));
    assert.equal(t.view.turn, 1);
  });

  test("unknown or unavailable moves are refused without spending the turn", () => {
    const { sessions } = setup();
    const a = start(sessions);
    assert.equal(sessions.turn({ battleId: a.battleId, action: { kind: "dance" } }).status, 400);
    assert.equal(sessions.turn({ battleId: a.battleId, action: { kind: "item", item: "elixir" } }).status, 400);
    assert.equal(sessions.turn({ battleId: "nope", action: { kind: "attack" } }).status, 404);
    // two salves to start with: the third is refused
    for (let i = 0; i < 2; i++) assert.equal(sessions.turn({ battleId: a.battleId, action: { kind: "item", item: "salve" } }).status, 200);
    const third = sessions.turn({ battleId: a.battleId, action: { kind: "item", item: "salve" } });
    assert.equal(third.status, 409);
    const next = sessions.turn({ battleId: a.battleId, action: { kind: "ward" } });
    assert.equal((next.body as TurnAnswer).view.turn, 3, "refused moves do not count as turns");
  });

  test("the seed is marked as used, so POST /battle can't count it again", () => {
    const { sessions } = setup(4242);
    const a = start(sessions);
    assert.ok(sessions.seen.has(`4242:${a.wild.id}:${DEFAULT_PLAYER_CURSE}`));
  });

  test("idle battles expire", () => {
    const { sessions, tick } = setup();
    const a = start(sessions);
    tick(BATTLE_IDLE_MS + 1);
    assert.equal(sessions.turn({ battleId: a.battleId, action: { kind: "attack" } }).status, 404);
  });

  test("too many open battles are refused", () => {
    const { sessions } = setup();
    for (let i = 0; i < MAX_OPEN_BATTLES; i++) start(sessions);
    assert.equal(sessions.start({ ward: "lantern" }).status, 503);
  });

  test("parseAction keeps only known shapes", () => {
    assert.deepEqual(parseAction({ kind: "seal", gilded: true, extra: 1 }), { kind: "seal", gilded: true });
    assert.deepEqual(parseAction({ kind: "seal" }), { kind: "seal" });
    assert.equal(parseAction({ kind: "seal", gilded: "yes" }), null);
    assert.equal(parseAction("attack"), null);
  });
});

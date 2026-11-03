// The web client's server battles (P2-C7): the local state the scene animates follows the server's view.
import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { createBattle, createRng, resolveTurn, createWorld } from "@grudgebound/shared";
import { BattleSessions, type StartAnswer, type TurnAnswer } from "../../server/src/battles";
import { syncState, type RemoteStart, type RemoteTurn } from "./battleClient";

describe("server battles in the web client", () => {
  it("syncState copies HP, items, turn and outcome from the server's view", () => {
    const s = createBattle({ wild: "weepwick", playerCurse: "cinder-grin" });
    const after = resolveTurn(s, { kind: "item", item: "salve" }, createRng(1)).state;
    const view = {
      turn: 3, outcome: "won" as const,
      player: { speciesId: "cinder-grin", name: "", emotion: s.player.emotion, omen: s.player.omen, hp: 7, maxHp: s.player.maxHp },
      wild: { speciesId: "weepwick", name: "", emotion: s.wild.emotion, omen: s.wild.omen, hp: 0, maxHp: s.wild.maxHp },
      items: { ...after.items },
      chances: { seal: 0, gildedSeal: 0, flee: 0 },
      canUse: { attack: false, ward: false, seal: false, gildedSeal: false, salve: false, flee: false },
    };
    const synced = syncState(s, view);
    assert.equal(synced.player.hp, 7);
    assert.equal(synced.wild.hp, 0);
    assert.equal(synced.turn, 3);
    assert.equal(synced.outcome, "won");
    assert.deepEqual(synced.items, after.items);
    assert.equal(synced.player.attack, s.player.attack, "stats the view doesn't carry are kept");
    assert.equal(s.player.hp, s.player.maxHp, "the old state is untouched");
  });

  it("the server's start and turn answers type-check as what the client reads, and stay in step", () => {
    const sessions = new BattleSessions(createWorld(7), { newSeed: () => 99, newId: () => "b1" });
    const start = sessions.start({ ward: "lantern", by: "Web" }).body as StartAnswer;
    const remote: RemoteStart = start;
    let state = syncState(createBattle({ wild: remote.wild.id, omen: remote.wild.omen, playerCurse: remote.view.player.speciesId }), remote.view);
    for (let i = 0; i < 200 && !state.outcome; i++) {
      const t = sessions.turn({ battleId: "b1", action: { kind: "attack" } }).body as TurnAnswer;
      const turn: RemoteTurn = t;
      state = syncState(state, turn.view);
      assert.equal(state.wild.hp, t.view.wild.hp);
      if (turn.outcome) assert.equal(typeof turn.seed, "number", "the seed is revealed at the end");
    }
    assert.ok(state.outcome, "the battle ended");
  });
});

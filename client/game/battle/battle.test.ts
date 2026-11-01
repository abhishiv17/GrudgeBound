// Run with: npm test   (Node's built-in test runner via tsx)
import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { CURSES, createRng, curseById } from "@grudgebound/shared";
import {
  createBattle, resolveTurn, replayBattle, rollDamage, sealChance, fleeChance, makeCombatant,
  defaultPlayerCurse, canUse, DEFAULT_PLAYER_CURSE, FLEE_MAX, FLEE_MIN, MIN_DAMAGE, STARTING_ITEMS,
  type PlayerAction,
} from "@grudgebound/shared";
import { describeEvent } from "./text";

const attacks = (n: number): PlayerAction[] => Array.from({ length: n }, () => ({ kind: "attack" }));

describe("battle setup", () => {
  it("uses species stats and a default bound curse", () => {
    const s = createBattle({ wild: "gutterjaw" });
    assert.equal(s.wild.maxHp, 72);
    assert.equal(s.wild.hp, 72);
    assert.equal(s.player.speciesId, DEFAULT_PLAYER_CURSE);
    assert.deepEqual(s.items, STARTING_ITEMS);
    assert.notEqual(defaultPlayerCurse(DEFAULT_PLAYER_CURSE), DEFAULT_PLAYER_CURSE);
  });
  it("lets the caller override omen and bound curse", () => {
    const s = createBattle({ wild: "weepwick", omen: "wail", playerCurse: "hushmoth" });
    assert.equal(s.wild.omen, "wail");
    assert.equal(s.player.speciesId, "hushmoth");
  });
});

describe("damage", () => {
  it("is at least MIN_DAMAGE for every pairing and respects the type triangle", () => {
    const rng = createRng(1);
    for (const a of CURSES) for (const d of CURSES) {
      assert.ok(rollDamage(makeCombatant(a), makeCombatant(d), rng).damage >= MIN_DAMAGE);
    }
    // Same seed, so the same variance and crit rolls: only the type multiplier differs.
    const rage = makeCombatant(curseById("knucklebrat"));
    const strong = rollDamage(rage, makeCombatant(curseById("weepwick")), createRng(5));
    const weak = rollDamage(rage, makeCombatant(curseById("hushmoth")), createRng(5));
    assert.equal(strong.multiplier, 1.5);
    assert.equal(weak.multiplier, 0.66);
    assert.ok(strong.damage > weak.damage);
  });
  it("is roughly halved by a ward", () => {
    const a = makeCombatant(curseById("riot-bell"));
    const d = makeCombatant(curseById("stairmouth"));
    const open = rollDamage(a, d, createRng(9)).damage;
    const warded = rollDamage(a, d, createRng(9), true).damage;
    assert.ok(Math.abs(warded - open / 2) <= 1);
  });
});

describe("seal and flee chances", () => {
  it("cannot seal a curse at full HP; wounded and gilded is easier", () => {
    const s = createBattle({ wild: "weepwick" });
    assert.equal(sealChance(s.wild), 0);
    s.wild.hp = Math.round(s.wild.maxHp * 0.3);
    assert.ok(sealChance(s.wild) > 0.5);
    assert.ok(sealChance(s.wild, true) > sealChance(s.wild));
    assert.ok(sealChance(s.wild, true) <= 0.95);
  });
  it("flee chance is clamped and harder against a calamity", () => {
    const s = createBattle({ wild: "hushmoth" });
    const p = fleeChance(s);
    assert.ok(p >= FLEE_MIN && p <= FLEE_MAX);
    const boss = createBattle({ wild: "riot-bell" });
    assert.ok(fleeChance(boss) < fleeChance({ ...boss, wild: { ...boss.wild, omen: "whisper" } }));
    assert.ok(fleeChance({ ...s, fleeAttempts: 2 }) > p);
  });
});

describe("turns", () => {
  it("does not mutate the input state", () => {
    const s = createBattle({ wild: "stairmouth" });
    const copy = structuredClone(s);
    resolveTurn(s, { kind: "attack" }, createRng(3));
    assert.deepEqual(s, copy);
  });
  it("uses up items and refuses actions that are not available", () => {
    const s = createBattle({ wild: "stairmouth", items: { salve: 1, gilded: 0 } });
    s.player.hp = 5;
    const t = resolveTurn(s, { kind: "item", item: "salve" }, createRng(3));
    assert.equal(t.state.items.salve, 0);
    assert.ok(t.events.some((e) => e.kind === "heal" && e.amount > 0));
    assert.equal(canUse(t.state, { kind: "item", item: "salve" }), false);
    assert.equal(canUse(t.state, { kind: "seal", gilded: true }), false);
    assert.throws(() => resolveTurn(t.state, { kind: "item", item: "salve" }, createRng(3)));
  });
  it("a successful seal ends the battle as bound", () => {
    // Search seeds deterministically for one where the seal lands.
    for (let seed = 1; seed < 200; seed++) {
      const s = createBattle({ wild: "weepwick" });
      s.wild.hp = 1;
      const t = resolveTurn(s, { kind: "seal" }, createRng(seed));
      if (t.state.outcome === "bound") {
        assert.equal(t.events.at(-1)?.kind, "end");
        return;
      }
    }
    assert.fail("no seed bound a 1-HP whisper in 200 tries");
  });
  it("attacking until someone falls ends in won or lost, and every event has a log line", () => {
    const turns = replayBattle({ wild: "weepwick" }, 42, attacks(50));
    const last = turns.at(-1)!;
    assert.ok(last.state.outcome === "won" || last.state.outcome === "lost");
    for (const t of turns) for (const e of t.events) assert.ok(describeEvent(e, t.state).length > 0);
  });
});

describe("replay", () => {
  it("same seed and actions give the same battle; a different seed differs", () => {
    const actions: PlayerAction[] = [{ kind: "attack" }, { kind: "ward" }, { kind: "attack" }, { kind: "seal" }, ...attacks(10)];
    const a = replayBattle({ wild: "ninefold-shade" }, 777, actions);
    const b = replayBattle({ wild: "ninefold-shade" }, 777, actions);
    assert.deepEqual(a, b);
    const c = replayBattle({ wild: "ninefold-shade" }, 778, actions);
    assert.notDeepEqual(a.map((t) => t.events), c.map((t) => t.events));
  });
});

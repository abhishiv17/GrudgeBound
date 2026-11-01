// Run with: npm test   (Node's built-in test runner via tsx; no extra test framework)
import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  typeMultiplier, CURSES, bindChance, createRng,
  emptyGrudge, addGrudge, removeGrudge, totalGrudge, rollSpawnOmen,
  quoteSellPrice, quoteBuyPrice, type MarketItem,
  pvpTake, ruleCost, arbiterCheck, renderRule, isCruel, type Rule,
  simulate, summarize,
  capturePoints, ruleMultiplier, curfewForbids, recordCapture, createWorld, totalGrudge as total,
  createBattle, resolveTurn, replayBattle, type PlayerAction,
} from "./index";

describe("type triangle", () => {
  it("Rage burns Sorrow, Sorrow drowns Dread, Dread freezes Rage", () => {
    assert.equal(typeMultiplier("rage", "sorrow"), 1.5);
    assert.equal(typeMultiplier("sorrow", "dread"), 1.5);
    assert.equal(typeMultiplier("dread", "rage"), 1.5);
    assert.equal(typeMultiplier("sorrow", "rage"), 0.66);
    assert.equal(typeMultiplier("rage", "rage"), 1);
  });
});

describe("curses", () => {
  it("has 12 curses: 4 per emotion, and one Calamity per emotion", () => {
    assert.equal(CURSES.length, 12);
    for (const e of ["rage", "sorrow", "dread"] as const) {
      assert.equal(CURSES.filter((c) => c.emotion === e).length, 4);
      assert.equal(CURSES.filter((c) => c.emotion === e && c.omen === "calamity").length, 1);
    }
  });
  it("binding is easier on weaker, more wounded curses", () => {
    assert.ok(bindChance("whisper", 0.2) > bindChance("calamity", 0.2));
    assert.ok(bindChance("wail", 0.1) > bindChance("wail", 0.9));
    assert.ok(bindChance("whisper", 0) <= 0.95);
  });
});

describe("grudge", () => {
  it("never exceeds 100 in total and never goes negative", () => {
    let g = emptyGrudge();
    g = addGrudge(g, "rage", 80);
    g = addGrudge(g, "sorrow", 80);
    assert.ok(Math.abs(totalGrudge(g) - 100) < 1e-9);
    g = removeGrudge(g, 500, "rage");
    assert.equal(g.rage, 0);
  });
  it("only Whispers spawn below 30", () => {
    const rng = createRng(1);
    for (let i = 0; i < 200; i++) assert.equal(rollSpawnOmen(29, rng), "whisper");
  });
});

describe("pricing", () => {
  const item: MarketItem = { id: "x", name: "Ward charms", base: 20, stock: 8, targetStock: 8, buys24h: 0, sells24h: 0, protective: true };
  const calm = { wardGrudge: 0, buyerIsOathbreaker: false };
  it("is the base price when stock, demand and fear are neutral", () => {
    assert.equal(quoteSellPrice(item, calm).price, 20);
  });
  it("rises with scarcity, fear and broken vows; falls when flooded", () => {
    assert.ok(quoteSellPrice({ ...item, stock: 2 }, calm).price > 20);
    assert.ok(quoteSellPrice(item, { ...calm, wardGrudge: 80 }).price > 20);
    assert.equal(quoteSellPrice(item, { ...calm, buyerIsOathbreaker: true }).price, 25);
    assert.ok(quoteSellPrice({ ...item, stock: 30 }, calm).price < 20);
  });
  it("merchants buy for less than they sell, and explain why", () => {
    const q = quoteSellPrice({ ...item, stock: 2 }, calm);
    assert.ok(quoteBuyPrice({ ...item, stock: 2 }, calm).price < q.price);
    assert.equal(q.reason, "Only 2 ward charms left.");
  });
});

describe("points and rules", () => {
  it("PvP takes 10% (min 5), never more than the loser has", () => {
    assert.equal(pvpTake(200), 20);
    assert.equal(pvpTake(20), 5);
    assert.equal(pvpTake(3), 3);
  });
  it("rule cost grows by 25 per active rule", () => {
    assert.equal(ruleCost(0), 50);
    assert.equal(ruleCost(4), 150);
  });
  it("renders template text", () => {
    assert.equal(renderRule("R8", { ward: "lantern" }), "No PvP in Lantern Ward");
  });
  it("the Arbiter rejects bad parameters, duplicates and contradictions", () => {
    const noPvp: Rule = { id: 1, templateId: "R8", params: { ward: "lantern" }, author: "a", cost: 50, addedAtTick: 0 };
    assert.equal(arbiterCheck("R8", { ward: "nowhere" }, []).accepted, false);
    assert.equal(arbiterCheck("R8", { ward: "lantern" }, [noPvp]).accepted, false);
    assert.equal(arbiterCheck("R1", { action: "pvp", ward: "lantern", multiplier: "2" }, [noPvp]).accepted, false);
    assert.equal(arbiterCheck("R1", { action: "pvp", ward: "ash", multiplier: "2" }, [noPvp]).accepted, true);
  });
  it("flags cruel rules", () => {
    assert.equal(isCruel("R7", { percent: 5, payee: "author" }), true);
    assert.equal(isCruel("R7", { percent: 5, payee: "burn" }), false);
  });
});

describe("engine", () => {
  it("is deterministic: same seed, same week", () => {
    const a = summarize(simulate(42, 24 * 7));
    const b = summarize(simulate(42, 24 * 7));
    assert.deepEqual(a, b);
  });
  it("a simulated week changes the world", () => {
    const s = summarize(simulate(42, 24 * 7));
    assert.equal(s.day, 8);
    assert.ok(s.leaderboard[0]!.points > 0);
  });
});

describe("player captures", () => {
  const bonus = { templateId: "R1", params: { action: "bind", ward: "lantern", multiplier: "2" } };
  const curfew = { templateId: "R2", params: { action: "exorcise", ward: "lantern", time: "night" } };

  it("pays the plan's points per omen, doubled by a matching R1 rule only", () => {
    assert.equal(capturePoints([], "bind", "lantern", "whisper"), 5);
    assert.equal(capturePoints([], "exorcise", "lantern", "calamity"), 80);
    assert.equal(capturePoints([bonus], "bind", "lantern", "wail"), 40);
    assert.equal(capturePoints([bonus], "exorcise", "lantern", "wail"), 20);
    assert.equal(capturePoints([bonus], "bind", "ash", "wail"), 20);
    assert.equal(ruleMultiplier([bonus, bonus], "bind", "lantern"), 4);
  });

  it("applies curfews only at the named time", () => {
    const night = 23, noon = 12;
    assert.equal(curfewForbids([curfew], "exorcise", "lantern", night), true);
    assert.equal(curfewForbids([curfew], "exorcise", "lantern", noon), false);
    assert.equal(curfewForbids([curfew], "bind", "lantern", night), false);
  });

  it("drains the Ward's Grudge, logs the capture and returns its points", () => {
    const world = createWorld(1);
    world.wards.lantern.grudge = { rage: 0, sorrow: 30, dread: 0 };
    const before = total(world.wards.lantern.grudge);
    const events = world.events.length;
    const pts = recordCapture(world, { ward: "lantern", speciesId: "weepwick", omen: "whisper", verb: "bind", by: "Tester" });
    assert.equal(pts, 5);
    assert.ok(total(world.wards.lantern.grudge) < before);
    assert.equal(world.events.length, events + 1);
    assert.match(world.events.at(-1)!.text, /Tester bound Weepwick in Lantern Ward \(\+5 points\)/);
  });

  it("a reported battle replays to the same outcome from its seed and actions", () => {
    const setup = { wild: "weepwick", playerCurse: "cinder-grin" };
    const rng = createRng(77);
    let state = createBattle(setup);
    const actions: PlayerAction[] = [];
    while (!state.outcome && actions.length < 50) {
      const a: PlayerAction = state.wild.hp / state.wild.maxHp < 0.4 ? { kind: "seal" } : { kind: "attack" };
      actions.push(a);
      state = resolveTurn(state, a, rng).state;
    }
    const replay = replayBattle(setup, 77, actions);
    assert.equal(replay.at(-1)!.state.outcome, state.outcome);
    const otherSeed = replayBattle(setup, 78, actions);
    assert.notDeepEqual(otherSeed.map((t) => t.events), replay.map((t) => t.events));
  });
});

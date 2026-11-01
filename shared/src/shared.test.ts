import { describe, expect, it } from "vitest";
import {
  typeMultiplier, CURSES, bindChance, createRng,
  emptyGrudge, addGrudge, removeGrudge, totalGrudge, rollSpawnOmen,
  quoteSellPrice, quoteBuyPrice, type MarketItem,
  pvpTake, ruleCost, arbiterCheck, renderRule, isCruel, type Rule,
  simulate, summarize,
} from "./index";

describe("type triangle", () => {
  it("Rage burns Sorrow, Sorrow drowns Dread, Dread freezes Rage", () => {
    expect(typeMultiplier("rage", "sorrow")).toBe(1.5);
    expect(typeMultiplier("sorrow", "dread")).toBe(1.5);
    expect(typeMultiplier("dread", "rage")).toBe(1.5);
    expect(typeMultiplier("sorrow", "rage")).toBe(0.66);
    expect(typeMultiplier("rage", "rage")).toBe(1);
  });
});

describe("curses", () => {
  it("has 12 curses: 4 per emotion, and one Calamity per emotion", () => {
    expect(CURSES).toHaveLength(12);
    for (const e of ["rage", "sorrow", "dread"] as const) {
      expect(CURSES.filter((c) => c.emotion === e)).toHaveLength(4);
      expect(CURSES.filter((c) => c.emotion === e && c.omen === "calamity")).toHaveLength(1);
    }
  });
  it("binding is easier on weaker, more wounded curses", () => {
    expect(bindChance("whisper", 0.2)).toBeGreaterThan(bindChance("calamity", 0.2));
    expect(bindChance("wail", 0.1)).toBeGreaterThan(bindChance("wail", 0.9));
    expect(bindChance("whisper", 0)).toBeLessThanOrEqual(0.95);
  });
});

describe("grudge", () => {
  it("never exceeds 100 in total and never goes negative", () => {
    let g = emptyGrudge();
    g = addGrudge(g, "rage", 80);
    g = addGrudge(g, "sorrow", 80);
    expect(totalGrudge(g)).toBeCloseTo(100);
    g = removeGrudge(g, 500, "rage");
    expect(g.rage).toBe(0);
  });
  it("only Whispers spawn below 30", () => {
    const rng = createRng(1);
    for (let i = 0; i < 200; i++) expect(rollSpawnOmen(29, rng)).toBe("whisper");
  });
});

describe("pricing", () => {
  const item: MarketItem = { id: "x", name: "Ward charms", base: 20, stock: 8, targetStock: 8, buys24h: 0, sells24h: 0, protective: true };
  const calm = { wardGrudge: 0, buyerIsOathbreaker: false };
  it("is the base price when stock, demand and fear are neutral", () => {
    expect(quoteSellPrice(item, calm).price).toBe(20);
  });
  it("rises with scarcity, fear and broken vows; falls when flooded", () => {
    expect(quoteSellPrice({ ...item, stock: 2 }, calm).price).toBeGreaterThan(20);
    expect(quoteSellPrice(item, { ...calm, wardGrudge: 80 }).price).toBeGreaterThan(20);
    expect(quoteSellPrice(item, { ...calm, buyerIsOathbreaker: true }).price).toBe(25);
    expect(quoteSellPrice({ ...item, stock: 30 }, calm).price).toBeLessThan(20);
  });
  it("merchants buy for less than they sell, and explain why", () => {
    const q = quoteSellPrice({ ...item, stock: 2 }, calm);
    expect(quoteBuyPrice({ ...item, stock: 2 }, calm).price).toBeLessThan(q.price);
    expect(q.reason).toBe("Only 2 ward charms left.");
  });
});

describe("points and rules", () => {
  it("PvP takes 10% (min 5), never more than the loser has", () => {
    expect(pvpTake(200)).toBe(20);
    expect(pvpTake(20)).toBe(5);
    expect(pvpTake(3)).toBe(3);
  });
  it("rule cost grows by 25 per active rule", () => {
    expect(ruleCost(0)).toBe(50);
    expect(ruleCost(4)).toBe(150);
  });
  it("renders template text", () => {
    expect(renderRule("R8", { ward: "lantern" })).toBe("No PvP in Lantern Ward");
  });
  it("the Arbiter rejects bad parameters, duplicates and contradictions", () => {
    const noPvp: Rule = { id: 1, templateId: "R8", params: { ward: "lantern" }, author: "a", cost: 50, addedAtTick: 0 };
    expect(arbiterCheck("R8", { ward: "nowhere" }, []).accepted).toBe(false);
    expect(arbiterCheck("R8", { ward: "lantern" }, [noPvp]).accepted).toBe(false);
    expect(arbiterCheck("R1", { action: "pvp", ward: "lantern", multiplier: "2" }, [noPvp]).accepted).toBe(false);
    expect(arbiterCheck("R1", { action: "pvp", ward: "ash", multiplier: "2" }, [noPvp]).accepted).toBe(true);
  });
  it("flags cruel rules", () => {
    expect(isCruel("R7", { percent: 5, payee: "author" })).toBe(true);
    expect(isCruel("R7", { percent: 5, payee: "burn" })).toBe(false);
  });
});

describe("engine", () => {
  it("is deterministic: same seed, same week", () => {
    const a = summarize(simulate(42, 24 * 7));
    const b = summarize(simulate(42, 24 * 7));
    expect(a).toEqual(b);
  });
  it("a simulated week changes the world", () => {
    const s = summarize(simulate(42, 24 * 7));
    expect(s.day).toBe(8);
    expect(s.leaderboard[0]!.points).toBeGreaterThan(0);
  });
});

/**
 * Grudge: each Ward's meter of negative emotion (0-100 in total), split into Rage, Sorrow and Dread.
 * The economy feeds it; exorcisms drain it; its level and mix decide which curses spawn.
 * Plan section 7.1.
 */
import { EMOTIONS, type Emotion } from "./emotions";
import type { Omen } from "./curses";
import type { Rng } from "./rng";
import { TICKS_PER_DAY } from "./time";

export type GrudgeMix = Record<Emotion, number>;

export const GRUDGE_MAX = 100;
/** Grudge fades by 3% per in-game day when nothing happens. */
export const DAILY_DECAY = 0.97;
export const TICK_DECAY = Math.pow(DAILY_DECAY, 1 / TICKS_PER_DAY);

/** What adds Grudge, how much, and to which emotion. */
export const GRUDGE_SOURCES = {
  bankruptcy: { amount: 15, emotion: "sorrow" },
  pvpWin: { amount: 1, emotion: "rage" },
  scam: { amount: 3, emotion: "rage" },
  /** Arbiter-flagged cruel rule passes (5 to 10 depending on severity). */
  cruelRulePassed: { amount: 7, emotion: "dread" },
  vowBroken: { amount: 4, emotion: "dread" },
} as const satisfies Record<string, { amount: number; emotion: Emotion }>;

/** Each binding of one emotion beyond this many per Ward per day adds +1 of that emotion. */
export const DAILY_HUNT_QUOTA = 6;

/** What removes Grudge. */
export const GRUDGE_SINKS = {
  exorcise: { whisper: 1, wail: 3, calamity: 10 } as Record<Omen, number>,
  bountyCompleted: 2,
  shopReopened: 8,
  cruelRuleRepealed: 5,
} as const;

export function emptyGrudge(): GrudgeMix {
  return { rage: 0, sorrow: 0, dread: 0 };
}

export function totalGrudge(g: GrudgeMix): number {
  return g.rage + g.sorrow + g.dread;
}

/** Keep every emotion >= 0 and the total <= 100 (scaled down proportionally). */
export function normalize(g: GrudgeMix): GrudgeMix {
  const out = { rage: Math.max(0, g.rage), sorrow: Math.max(0, g.sorrow), dread: Math.max(0, g.dread) };
  const total = totalGrudge(out);
  if (total > GRUDGE_MAX) {
    const k = GRUDGE_MAX / total;
    for (const e of EMOTIONS) out[e] *= k;
  }
  return out;
}

export function addGrudge(g: GrudgeMix, emotion: Emotion, amount: number): GrudgeMix {
  return normalize({ ...g, [emotion]: g[emotion] + amount });
}

/** Remove Grudge; when no emotion is given, take it from the largest one. */
export function removeGrudge(g: GrudgeMix, amount: number, emotion?: Emotion): GrudgeMix {
  const target = emotion ?? EMOTIONS.reduce((a, b) => (g[a] >= g[b] ? a : b));
  return normalize({ ...g, [target]: g[target] - amount });
}

export function decayTick(g: GrudgeMix): GrudgeMix {
  return normalize({ rage: g.rage * TICK_DECAY, sorrow: g.sorrow * TICK_DECAY, dread: g.dread * TICK_DECAY });
}

/** Grudge level thresholds for spawns. */
export const WAIL_THRESHOLD = 30;
export const CALAMITY_THRESHOLD = 70;

/**
 * Omen of a regular spawn roll.
 * 0-29: only Whispers. 30-69 (and above): Wail with chance (Grudge - 30) / 80.
 * Calamities are not rolled here: the engine raises at most one per Ward per day once Grudge >= 70.
 */
export function rollSpawnOmen(total: number, rng: Rng): Omen {
  if (total < WAIL_THRESHOLD) return "whisper";
  const wailChance = Math.min(1, (total - WAIL_THRESHOLD) / 80);
  return rng.chance(wailChance) ? "wail" : "whisper";
}

/** Emotion of a spawn: weighted by the Ward's mix, plus a small baseline so every type can appear. */
export function rollSpawnEmotion(g: GrudgeMix, rng: Rng, baseline = 2): Emotion {
  return rng.weighted(EMOTIONS, (e) => g[e] + baseline);
}

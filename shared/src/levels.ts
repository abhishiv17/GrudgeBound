/**
 * Partner levels and the rival. Server-owned numbers: the client is told a talisman's level, never asked for it.
 * A battle freezes both levels into its starting setup, so a replay needs nothing but that setup.
 * Numbers are a draft for balance review (docs/STORY.md, "Levels and the rival").
 */
import type { Emotion } from "./emotions";
import type { WardId } from "./wards";

export const MAX_LEVEL = 10;
/** Experience needed to reach each level (index = level; levels 0 and 1 need none). */
export const LEVEL_XP = [0, 0, 20, 45, 75, 110, 150, 195, 245, 300, 360] as const;

export function validLevel(level: unknown): level is number {
  return Number.isSafeInteger(level) && (level as number) >= 1 && (level as number) <= MAX_LEVEL;
}

export function levelForXp(xp: number): number {
  let level = 1;
  while (level < MAX_LEVEL && xp >= LEVEL_XP[level + 1]!) level++;
  return level;
}

/** Experience for the next level, or null at the cap. */
export function nextLevelXp(level: number): number | null {
  return level >= MAX_LEVEL ? null : LEVEL_XP[level + 1]!;
}

export interface StatGrowth { hp: number; attack: number; guard: number; speed: number }

/** What a partner gains over level 1. Rage grows its bite, Sorrow its endurance, Dread its speed. */
export function growth(emotion: Emotion, level: number): StatGrowth {
  const n = Math.max(0, Math.min(MAX_LEVEL, level) - 1);
  switch (emotion) {
    case "rage": return { hp: 3 * n, attack: Math.floor(1.5 * n), guard: Math.floor(0.5 * n), speed: Math.floor(0.5 * n) };
    case "sorrow": return { hp: 4 * n, attack: n, guard: n, speed: Math.floor(0.5 * n) };
    case "dread": return { hp: 3 * n, attack: n, guard: Math.floor(0.5 * n), speed: n };
  }
}

/** Experience for a won or bound battle. Fled and lost battles give none. */
export function xpForWin(foeLevel: number, rivalStage?: number): number {
  return rivalStage ? 25 + 15 * rivalStage : 10 + 3 * foeLevel;
}

/** Wild curses are met near the partner's level, inside each Ward's range. */
export const WARD_LEVELS: Record<WardId, readonly [number, number]> = { lantern: [1, 4], tide: [3, 7], ash: [4, 8], bone: [5, 9] };

/** `roll` in [0, 1): the partner's level or one below, kept inside the Ward's range. */
export function wildLevel(ward: WardId, partnerLevel: number, roll: number): number {
  const [min, max] = WARD_LEVELS[ward];
  return Math.max(min, Math.min(max, partnerLevel - 1 + Math.floor(roll * 2)));
}

// ------------------------------------------------------------------ the rival (draft name)

export const RIVAL_NAME = "Dace Marrow";
export const RIVAL_STAGES = 3;
export interface RivalStage {
  stage: number;
  ward: WardId;
  level: number;
  /** Points written to the Rite for the win. */
  points: number;
}
/**
 * Simulated with the shared resolver (attack, salve under 35% ink), the player wins about: stage 1 at level 1, ~100%;
 * stage 2 at level 4, 95-100% (level 3, 34-90%); stage 3 at level 7 with Etched Focus, 91-100% (level 6, 50-99%).
 */
export const RIVAL_STAGE: Record<number, RivalStage> = {
  1: { stage: 1, ward: "lantern", level: 1, points: 10 },
  2: { stage: 2, ward: "lantern", level: 4, points: 20 },
  3: { stage: 3, ward: "tide", level: 7, points: 30 },
};

/**
 * The rival chose the same first partner as you, to prove they can do more with it. A mirror keeps it a test of growth:
 * the emotion triangle (x1.5 / x0.66) would make a counter-pick unwinnable at equal level.
 */
export function rivalPartner(playerStarter: string): string {
  return playerStarter;
}

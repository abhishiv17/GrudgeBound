import type { Emotion } from "./emotions";
import type { WardId } from "./wards";

export const OMENS = ["whisper", "wail", "calamity"] as const;
export type Omen = (typeof OMENS)[number];

export interface CurseStats {
  hp: number;
  attack: number;
  guard: number;
  speed: number;
}

export interface CurseSpecies {
  id: string;
  name: string;
  emotion: Emotion;
  omen: Omen;
  /** Ward whose materials it is made of (art direction), and where it spawns most. */
  home: WardId;
  bornFrom: string;
  signatureMove: { name: string; effect: string };
  stats: CurseStats;
}

const S = (hp: number, attack: number, guard: number, speed: number): CurseStats => ({ hp, attack, guard, speed });

/** The 12 original curses (plan, Appendix A). */
export const CURSES: CurseSpecies[] = [
  { id: "cinder-grin", name: "Cinder Grin", emotion: "rage", omen: "whisper", home: "ash", bornFrom: "street brawls",
    signatureMove: { name: "Spark Laugh", effect: "burns over time" }, stats: S(38, 12, 6, 11) },
  { id: "knucklebrat", name: "Knucklebrat", emotion: "rage", omen: "whisper", home: "ash", bornFrom: "old schoolyard grudges",
    signatureMove: { name: "Cheap Shot", effect: "always hits first" }, stats: S(42, 11, 8, 9) },
  { id: "gutterjaw", name: "Gutterjaw", emotion: "rage", omen: "wail", home: "tide", bornFrom: "scams and broken deals",
    signatureMove: { name: "Swindle", effect: "steals the opponent's boost" }, stats: S(72, 17, 11, 10) },
  { id: "riot-bell", name: "Riot Bell", emotion: "rage", omen: "calamity", home: "ash", bornFrom: "a Ward where PvP never stops",
    signatureMove: { name: "Toll of War", effect: "hits all foes" }, stats: S(150, 24, 16, 8) },

  { id: "lantern-drowned", name: "Lantern Drowned", emotion: "sorrow", omen: "whisper", home: "tide", bornFrom: "lost cargo at the docks",
    signatureMove: { name: "Undertow", effect: "lowers Speed" }, stats: S(40, 9, 9, 10) },
  { id: "weepwick", name: "Weepwick", emotion: "sorrow", omen: "whisper", home: "lantern", bornFrom: "unpaid wages",
    signatureMove: { name: "Gutter Candle", effect: "heals self" }, stats: S(44, 8, 10, 8) },
  { id: "tallow-widow", name: "Tallow Widow", emotion: "sorrow", omen: "wail", home: "lantern", bornFrom: "debts never repaid",
    signatureMove: { name: "Wax Veil", effect: "blocks two hits" }, stats: S(78, 13, 16, 7) },
  { id: "the-shuttered", name: "The Shuttered", emotion: "sorrow", omen: "calamity", home: "lantern", bornFrom: "a bankrupt shop",
    signatureMove: { name: "Closing Time", effect: "seals one enemy move" }, stats: S(160, 20, 22, 6) },

  { id: "hushmoth", name: "Hushmoth", emotion: "dread", omen: "whisper", home: "bone", bornFrom: "rumors at night",
    signatureMove: { name: "Hush", effect: "enemy skips a turn" }, stats: S(34, 10, 7, 13) },
  { id: "stairmouth", name: "Stairmouth", emotion: "dread", omen: "whisper", home: "bone", bornFrom: "abandoned archives",
    signatureMove: { name: "Misstep", effect: "confuses" }, stats: S(46, 10, 9, 7) },
  { id: "ninefold-shade", name: "Ninefold Shade", emotion: "dread", omen: "wail", home: "bone", bornFrom: "curfews and tolls",
    signatureMove: { name: "Nine Echoes", effect: "repeats its last move" }, stats: S(70, 15, 12, 12) },
  { id: "curfew-king", name: "Curfew King", emotion: "dread", omen: "calamity", home: "bone", bornFrom: "five or more cruel rules in force",
    signatureMove: { name: "Decree", effect: "reverses the type triangle for 3 turns" }, stats: S(155, 22, 18, 9) },
];

export function curseById(id: string): CurseSpecies {
  const c = CURSES.find((x) => x.id === id);
  if (!c) throw new Error(`Unknown curse: ${id}`);
  return c;
}

export function cursesWhere(emotion: Emotion, omen: Omen): CurseSpecies[] {
  return CURSES.filter((c) => c.emotion === emotion && c.omen === omen);
}

/** Base chance to bind, before HP, talisman quality and vow bonuses. */
export const BIND_BASE: Record<Omen, number> = { whisper: 0.9, wail: 0.6, calamity: 0.3 };

/**
 * Binding chance = base(Omen) x (1 - HP%) x talisman quality x vow bonus, capped at 0.95.
 * hpFraction: remaining HP / max HP (0..1).
 */
export function bindChance(omen: Omen, hpFraction: number, talismanQuality = 1, vowBonus = 1): number {
  const hpFactor = 1 - Math.min(1, Math.max(0, hpFraction));
  return Math.min(0.95, BIND_BASE[omen] * hpFactor * talismanQuality * vowBonus);
}

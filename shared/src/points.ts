import type { Omen } from "./curses";

/** Points for exorcising or binding a curse (plan 7.4). */
export const POINTS_FOR_OMEN: Record<Omen, number> = { whisper: 5, wail: 20, calamity: 80 };

export const PVP_TAKE_RATIO = 0.1;
export const PVP_TAKE_MIN = 5;

/** Points the winner takes from the loser. Never more than the loser has. */
export function pvpTake(loserPoints: number): number {
  return Math.min(loserPoints, Math.max(PVP_TAKE_MIN, Math.floor(loserPoints * PVP_TAKE_RATIO)));
}

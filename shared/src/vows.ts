/**
 * Vows: accept a limit, gain power. Enforced by the Rite contract.
 * Kept: stake returned +20%. Broken: stake burned, Oathbreaker for 1 day, Dread +4.
 * Plan section 7.6 and Appendix C.
 */
export const VOW_KEEP_BONUS = 0.2;
export const OATHBREAKER_DAYS = 1;

export interface VowTemplate {
  id: string;
  text: string;
  durationDays: [min: number, max: number];
  power: string;
  stake: number;
  mvp: boolean;
}

export const VOW_TEMPLATES: VowTemplate[] = [
  { id: "V1", text: "I will not leave {ward}", durationDays: [1, 3], power: "+20% attack for all your curses", stake: 20, mvp: true },
  { id: "V2", text: "I will not use healing items", durationDays: [1, 2], power: "+15% binding chance", stake: 15, mvp: true },
  { id: "V3", text: "I will not sell {talisman}", durationDays: [3, 3], power: "that talisman +1 level, purse +10%", stake: 10, mvp: true },
  { id: "V4", text: "I will fight only {omen} curses", durationDays: [1, 1], power: "points from those curses x1.5", stake: 25, mvp: true },
  { id: "V5", text: "I will not challenge players", durationDays: [2, 2], power: "merchants give you 10% better prices", stake: 10, mvp: false },
  { id: "V6", text: "I will exorcise {count} curses today", durationDays: [1, 1], power: "double bounty rewards if kept", stake: 30, mvp: false },
];

export function vowPayout(stake: number, kept: boolean): number {
  return kept ? Math.round(stake * (1 + VOW_KEEP_BONUS)) : 0;
}

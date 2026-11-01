/** 1 tick = 1 in-game hour = 1 real minute (by default). */
export const TICKS_PER_DAY = 24;
export const DEFAULT_REAL_MS_PER_TICK = 60_000;

export function dayOf(tick: number): number {
  return Math.floor(tick / TICKS_PER_DAY) + 1;
}

export function hourOf(tick: number): number {
  return tick % TICKS_PER_DAY;
}

/** Night runs from 20:00 to 05:59. Curfew rules and some Dread curses only act at night. */
export function isNight(tick: number): boolean {
  const h = hourOf(tick);
  return h >= 20 || h < 6;
}

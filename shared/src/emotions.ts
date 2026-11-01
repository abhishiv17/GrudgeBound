/**
 * The three emotions that curses are born from.
 * Type triangle: Rage burns Sorrow, Sorrow drowns Dread, Dread freezes Rage.
 */
export const EMOTIONS = ["rage", "sorrow", "dread"] as const;
export type Emotion = (typeof EMOTIONS)[number];

export const EMOTION_INFO: Record<Emotion, { label: string; color: string }> = {
  rage: { label: "Rage", color: "#C8402A" },
  sorrow: { label: "Sorrow", color: "#2B6CB0" },
  dread: { label: "Dread", color: "#6B46C1" },
};

const BEATS: Record<Emotion, Emotion> = {
  rage: "sorrow",
  sorrow: "dread",
  dread: "rage",
};

export const STRONG_HIT = 1.5;
export const WEAK_HIT = 0.66;

/** Damage multiplier when a curse of `attacker` emotion hits one of `defender` emotion. */
export function typeMultiplier(attacker: Emotion, defender: Emotion): number {
  if (BEATS[attacker] === defender) return STRONG_HIT;
  if (BEATS[defender] === attacker) return WEAK_HIT;
  return 1;
}

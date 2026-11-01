import type { Emotion } from "./emotions";

export const WARDS = ["lantern", "ash", "tide", "bone"] as const;
export type WardId = (typeof WARDS)[number];

export interface WardInfo {
  id: WardId;
  name: string;
  theme: string;
  /** Emotions this Ward leans toward when nothing else is happening. */
  leaning: Emotion[];
}

export const WARD_INFO: Record<WardId, WardInfo> = {
  lantern: { id: "lantern", name: "Lantern Ward", theme: "Night market, shops, gambling dens", leaning: ["sorrow"] },
  ash: { id: "ash", name: "Ash Ward", theme: "Foundries, fighting pits", leaning: ["rage"] },
  tide: { id: "tide", name: "Tide Ward", theme: "Docks, warehouses, smugglers", leaning: ["rage", "sorrow"] },
  bone: { id: "bone", name: "Bone Ward", theme: "Old temples, graveyard, archives", leaning: ["dread"] },
};

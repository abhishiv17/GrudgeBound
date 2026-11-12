/** Personal quest equipment. Effective bonuses are snapshotted in each battle record. */
export interface EquipmentSnapshot { version: 1; itemId: string; attack: number; guard: number }
export const GEAR = [
  { id: "worn-focus", name: "Worn Focus", attack: 1, guard: 0, source: "Return the Lamplighter's kit." },
  { id: "ward-clasp", name: "Ward Clasp", attack: 0, guard: 2, source: "Help the Net mender at Tide." },
  { id: "etched-focus", name: "Etched Focus", attack: 2, guard: 0, source: "Discover the sealed storehouse." },
] as const;
export function equipmentSnapshot(itemId: string): EquipmentSnapshot | undefined {
  const gear = GEAR.find(g => g.id === itemId);
  return gear ? { version: 1, itemId: gear.id, attack: gear.attack, guard: gear.guard } : undefined;
}

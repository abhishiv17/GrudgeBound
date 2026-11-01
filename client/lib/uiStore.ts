/**
 * Small UI state shared by the React overlay and the Phaser scene:
 * which panel is open, the current encounter, and rules the player sealed locally.
 * While anything here blocks play, the scene freezes the player and releases the keyboard
 * so form fields and buttons get every key.
 */
import type { CurseSpecies, RuleParams } from "@grudgebound/shared";

export type PanelId = "rules" | "vows";

/** A rule the player sealed in the Rewrite panel. Local only: nothing is sent to the Rite yet. */
export interface LocalRule {
  id: string;
  templateId: string;
  params: RuleParams;
  text: string;
  cost: number;
  cruel: boolean;
}

export interface UiState {
  panel: PanelId | null;
  encounter: CurseSpecies | null;
  sealed: LocalRule[];
  /** The player stands at the Rite notice board (press E to rewrite the rules). */
  nearBoard: boolean;
}

let state: UiState = { panel: null, encounter: null, sealed: [], nearBoard: false };
const listeners = new Set<() => void>();

function set(patch: Partial<UiState>) {
  state = { ...state, ...patch };
  for (const l of listeners) l();
}

export const uiStore = {
  subscribe(listener: () => void): () => void {
    listeners.add(listener);
    return () => listeners.delete(listener);
  },
  getSnapshot: (): UiState => state,
  openPanel: (panel: PanelId) => set({ panel }),
  closePanel: () => set({ panel: null }),
  openEncounter: (encounter: CurseSpecies) => set({ encounter }),
  closeEncounter: () => set({ encounter: null }),
  seal: (rule: LocalRule) => set({ sealed: [...state.sealed, rule] }),
  unseal: (id: string) => set({ sealed: state.sealed.filter((r) => r.id !== id) }),
  setNearBoard: (nearBoard: boolean) => { if (state.nearBoard !== nearBoard) set({ nearBoard }); },
};

export function isBlocking(s: UiState = state): boolean {
  return s.panel !== null || s.encounter !== null;
}

/** On-screen D-pad state for touch screens; the scene reads it every frame. */
export const touchInput = { up: false, down: false, left: false, right: false };

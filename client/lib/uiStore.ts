/**
 * Small UI state shared by the React overlay and the Phaser scene:
 * which panel is open, the current encounter and battle, and rules the player sealed locally.
 * While anything here blocks play, the scene freezes the player and releases the keyboard
 * so form fields and buttons get every key.
 */
import type { CurseSpecies, Omen, RuleParams } from "@grudgebound/shared";
import type { RemoteStart } from "../game/battleClient";

export type PanelId = "rules" | "vows" | "talismans";

/** A battle the player chose to fight. WardScene launches BattleScene from it. */
export interface BattleRequest {
  wild: string;
  omen: Omen;
  seed: number;
  /** Set when the server runs this battle (it already started it and picked the curse). */
  remote?: RemoteStart;
}

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
  /** Set while a battle is on screen. */
  battle: BattleRequest | null;
  /** The server battle behind the open encounter, if any. */
  remote: RemoteStart | null;
  sealed: LocalRule[];
  /** The player stands at the Rite notice board (press E to rewrite the rules). */
  nearBoard: boolean;
}

let state: UiState = { panel: null, encounter: null, battle: null, remote: null, sealed: [], nearBoard: false };
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
  openEncounter: (encounter: CurseSpecies, remote: RemoteStart | null = null) => set({ encounter, remote }),
  closeEncounter: () => set({ encounter: null, remote: null }),
  /** "Fight!" on the encounter dialog: the dialog closes and the battle starts. */
  startBattle: () => {
    const c = state.encounter;
    if (!c || state.battle) return;
    const remote = state.remote ?? undefined;
    set({ encounter: null, remote: null, battle: { wild: c.id, omen: remote?.wild.omen ?? c.omen, seed: crypto.getRandomValues(new Uint32Array(1))[0]!, remote } });
  },
  endBattle: () => set({ battle: null }),
  seal: (rule: LocalRule) => set({ sealed: [...state.sealed, rule] }),
  unseal: (id: string) => set({ sealed: state.sealed.filter((r) => r.id !== id) }),
  setNearBoard: (nearBoard: boolean) => { if (state.nearBoard !== nearBoard) set({ nearBoard }); },
};

export function isBlocking(s: UiState = state): boolean {
  return s.panel !== null || s.encounter !== null || s.battle !== null;
}

/** On-screen D-pad state for touch screens; the scene reads it every frame. */
export const touchInput = { up: false, down: false, left: false, right: false };

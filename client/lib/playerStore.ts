/**
 * The signed-in player's own progress: points, Ember, talismans, partner curse and battle record.
 *
 * Phase 1: kept in this browser (localStorage), so a refresh keeps your talismans.
 * Phase 2: the same shape is filled from the chain instead (talismans = NEP-245 tokens in the
 * items contract, points = the Rite contract), and recordBattle() becomes "report to the server".
 *
 * React reads it with useSyncExternalStore; the Phaser scenes call getSnapshot() directly.
 */
import {
  DEFAULT_PLAYER_CURSE, curseById, type CaptureVerb, type Omen, type Outcome,
} from "@grudgebound/shared";

export interface Talisman {
  /** Local id now; becomes the NEP-245 token id (tal-<n>) once minted. */
  id: string;
  speciesId: string;
  omen: Omen;
  /** In-game day it was bound; null for the starter Kessa gives you. */
  boundDay: number | null;
  /** Seed of the battle that bound it (replayable); null for the starter. */
  seed: number | null;
  /** Battles this talisman has won or bound a curse in. */
  wins: number;
}

export interface BattleRecord {
  won: number;
  bound: number;
  fled: number;
  lost: number;
}

/** One finished battle, newest first in PlayerState.history. */
export interface BattleEntry {
  outcome: Outcome;
  wild: string;
  omen: Omen;
  partner: string;
  points: number;
  seed: number;
  day: number;
}

export interface PlayerState {
  points: number;
  ember: number;
  talismans: Talisman[];
  /** Talisman id of the curse that fights for you. */
  partnerId: string;
  record: BattleRecord;
  history: BattleEntry[];
  nextTalisman: number;
}

const STORAGE_KEY = "grudgebound.player.v1";
let storageKey = STORAGE_KEY;
const MAX_HISTORY = 20;
/** Same purse every simulated participant starts with in the engine. */
export const STARTING_EMBER = 60;
export const STARTER_ID = "tal-starter";

function fresh(): PlayerState {
  return {
    points: 0,
    ember: STARTING_EMBER,
    talismans: [{ id: STARTER_ID, speciesId: DEFAULT_PLAYER_CURSE, omen: curseById(DEFAULT_PLAYER_CURSE).omen, boundDay: null, seed: null, wins: 0 }],
    partnerId: STARTER_ID,
    record: { won: 0, bound: 0, fled: 0, lost: 0 },
    history: [],
    nextTalisman: 1,
  };
}

const INITIAL = fresh();

function isValid(s: unknown): s is PlayerState {
  const p = s as PlayerState;
  return !!p && typeof p.points === "number" && Array.isArray(p.talismans) && p.talismans.length > 0
    && typeof p.partnerId === "string" && !!p.record && Array.isArray(p.history);
}

function load(): PlayerState {
  try {
    const raw = window.localStorage.getItem(storageKey);
    if (!raw) return fresh();
    const parsed: unknown = JSON.parse(raw);
    if (!isValid(parsed)) return fresh();
    // drop talismans of species that no longer exist (renamed in a later build)
    const talismans = parsed.talismans.filter((t) => { try { curseById(t.speciesId); return true; } catch { return false; } });
    if (!talismans.length) return fresh();
    const partnerId = talismans.some((t) => t.id === parsed.partnerId) ? parsed.partnerId : talismans[0]!.id;
    return { ...parsed, talismans, partnerId };
  } catch {
    return fresh();
  }
}

function save(s: PlayerState) {
  try {
    window.localStorage.setItem(storageKey, JSON.stringify(s));
  } catch {
    /* private window or storage full: progress lasts until the tab closes */
  }
}

let state: PlayerState | null = null;
const listeners = new Set<() => void>();

function current(): PlayerState {
  if (!state) state = typeof window === "undefined" ? INITIAL : load();
  return state;
}

function set(next: PlayerState) {
  state = next;
  save(next);
  for (const l of listeners) l();
}

export interface BattleReport {
  outcome: Outcome;
  wild: string;
  omen: Omen;
  seed: number;
  /** Points the capture earned under the rules in force (0 for fled / lost). */
  points: number;
  day: number;
}

/** "bind" / "exorcise" for the outcomes that take a curse out of the Ward, else null. */
export function captureVerb(outcome: Outcome): CaptureVerb | null {
  return outcome === "bound" ? "bind" : outcome === "won" ? "exorcise" : null;
}

export const playerStore = {
  selectProfile(playerId?: string) {
    const key = playerId ? `${STORAGE_KEY}.${playerId}` : STORAGE_KEY;
    if (key === storageKey) return;
    storageKey = key;
    state = load();
    for (const l of listeners) l();
  },
  subscribe(listener: () => void): () => void {
    listeners.add(listener);
    return () => listeners.delete(listener);
  },
  getSnapshot: current,
  getServerSnapshot: (): PlayerState => INITIAL,

  partner(): Talisman {
    const s = current();
    return s.talismans.find((t) => t.id === s.partnerId) ?? s.talismans[0]!;
  },

  /** Apply a finished battle: points, a new talisman on a binding, the partner's win count, the record. */
  recordBattle(r: BattleReport): Talisman | null {
    const s = current();
    const partner = playerStore.partner();
    const won = r.outcome === "won" || r.outcome === "bound";
    let bound: Talisman | null = null;
    let talismans = won ? s.talismans.map((t) => (t.id === partner.id ? { ...t, wins: t.wins + 1 } : t)) : s.talismans;
    let nextTalisman = s.nextTalisman;
    if (r.outcome === "bound") {
      bound = { id: `tal-${nextTalisman++}`, speciesId: r.wild, omen: r.omen, boundDay: r.day, seed: r.seed, wins: 0 };
      talismans = [...talismans, bound];
    }
    const entry: BattleEntry = { outcome: r.outcome, wild: r.wild, omen: r.omen, partner: partner.speciesId, points: won ? r.points : 0, seed: r.seed, day: r.day };
    set({
      ...s,
      points: s.points + entry.points,
      talismans,
      nextTalisman,
      record: { ...s.record, [r.outcome]: s.record[r.outcome] + 1 },
      history: [entry, ...s.history].slice(0, MAX_HISTORY),
    });
    return bound;
  },

  setPartner(id: string) {
    const s = current();
    if (s.partnerId !== id && s.talismans.some((t) => t.id === id)) set({ ...s, partnerId: id });
  },

  /** Start over with only the starter talisman (from the talisman panel). */
  reset() {
    set(fresh());
  },
};

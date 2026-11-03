/**
 * Server-run battles for the web client (P2-C7): the same routes the Unreal client uses.
 *
 *   POST /battle/start { ward, playerCurse, by, account? } -> the server picks the curse and a secret seed
 *   POST /battle/turn  { battleId, action }                -> the server resolves the turn and says what happened
 *
 * The client never sees the seed until the battle ends, so it can't retry seeds or claim a rarer curse.
 * BattleScene still animates from a local BattleState; syncState() keeps it equal to the server's view.
 */
import type { BattleEvent, BattleState, Combatant, Omen, Outcome } from "@grudgebound/shared";

export interface SideView {
  speciesId: string;
  name: string;
  emotion: Combatant["emotion"];
  omen: Omen;
  hp: number;
  maxHp: number;
}

export interface BattleView {
  turn: number;
  outcome: Outcome | null;
  player: SideView;
  wild: SideView;
  items: BattleState["items"];
  chances: { seal: number; gildedSeal: number; flee: number };
  canUse: { attack: boolean; ward: boolean; seal: boolean; gildedSeal: boolean; salve: boolean; flee: boolean };
}

export interface RemoteStart {
  battleId: string;
  ward: string;
  wild: { id: string; name: string; emotion: Combatant["emotion"]; omen: Omen };
  matchup: "advantage" | "disadvantage" | "even";
  view: BattleView;
  /** The intro line, then any curfew warning from Kessa. */
  lines: string[];
  curfews?: string[];
}

export interface RemoteTurn {
  battleId: string;
  action: string;
  events: BattleEvent[];
  /** One line per event, then any extra words from Kessa (a broken curfew). */
  lines: string[];
  view: BattleView;
  outcome: Outcome | null;
  points?: number;
  seed?: number;
  /** The curfew the ending broke (points were 0). */
  curfew?: string;
  chain?: { status: string; tokenId?: string; txs: { kind: string; url: string }[] };
}

export type Reply<T> = { ok: true; body: T } | { ok: false; status: number; error: string };

/** The server's view, copied onto the local state the scene animates (HP, items, turn, outcome). */
export function syncState(state: BattleState, view: BattleView): BattleState {
  return {
    ...state,
    player: { ...state.player, hp: view.player.hp, maxHp: view.player.maxHp },
    wild: { ...state.wild, hp: view.wild.hp, maxHp: view.wild.maxHp },
    items: { ...view.items },
    turn: view.turn,
    outcome: view.outcome,
  };
}

export async function postReply<T>(url: string, body: unknown): Promise<Reply<T>> {
  try {
    const res = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
    const data = (await res.json().catch(() => ({}))) as T & { error?: string };
    if (!res.ok) return { ok: false, status: res.status, error: data.error ?? `server said ${res.status}` };
    return { ok: true, body: data };
  } catch (e) {
    return { ok: false, status: 0, error: e instanceof Error ? e.message : "the town did not answer" };
  }
}

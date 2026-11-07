/**
 * Server-run battles (P1-C5), for clients that should not carry their own copy of the battle rules
 * (the Unreal client). The server picks the curse from the Ward's Grudge, makes the seed, and resolves
 * every turn with the shared resolver. The client only sends moves and shows what happened.
 *
 *   POST /battle/start  { ward, playerCurse?, by?, account? } -> the curse met, the battle view, the intro line
 *                       (account: the NEAR account that gets the talisman and points, see chain.ts)
 *   POST /battle/turn   { battleId, action }         -> this turn's events and log lines, the new view;
 *                                                       when the battle ends: outcome, points, seed
 *
 * The seed stays secret until the battle ends: a client that knew it could simulate each move ahead
 * and only Seal when the roll would succeed. At the end it is returned so anyone can replay the battle.
 *
 * Trust today: playerCurse is taken as sent. Phase 2 checks it against the player's talismans on chain.
 */
import { randomInt, randomUUID } from "node:crypto";
import {
  CURSES, DEFAULT_PLAYER_CURSE, WARDS,
  actionLabel, canUse, createBattle, createRng, curseById, describeEvent, fleeChance, introLine,
  curfewText, recordCapture, resolveTurn, rollEncounter, sealChance, typeMultiplier,
  type BattleEvent, type BattleState, type Combatant, type Omen, type Outcome, type PlayerAction,
  type Rng, type WardId, type World,
} from "@grudgebound/shared";
import { isAccountId } from "./chain";

/** A battle nobody has touched for this long is dropped (the player walked away). */
export const BATTLE_IDLE_MS = 15 * 60_000;
/** Open battles kept at once; more are refused until old ones expire. */
export const MAX_OPEN_BATTLES = 500;
export const MAX_TURNS = 200;

export interface SideView {
  speciesId: string;
  name: string;
  emotion: Combatant["emotion"];
  omen: Omen;
  hp: number;
  maxHp: number;
}

/** Everything a battle screen needs to draw the current turn. */
export interface BattleView {
  turn: number;
  outcome: Outcome | null;
  player: SideView;
  wild: SideView;
  items: BattleState["items"];
  /** Chances shown on the buttons, 0-1. */
  chances: { seal: number; gildedSeal: number; flee: number };
  canUse: { attack: boolean; ward: boolean; seal: boolean; gildedSeal: boolean; salve: boolean; flee: boolean };
}

export interface StartAnswer {
  battleId: string;
  ward: WardId;
  wild: { id: string; name: string; emotion: Combatant["emotion"]; omen: Omen };
  /** How your partner's grudge fares against this curse. */
  matchup: "advantage" | "disadvantage" | "even";
  view: BattleView;
  lines: string[];
  /** Curfews (R2) in force here right now, as rule text: "exorcise is forbidden in Lantern Ward during the night". */
  curfews: string[];
}

export interface TurnAnswer {
  battleId: string;
  action: string;
  events: BattleEvent[];
  lines: string[];
  view: BattleView;
  outcome: Outcome | null;
  /** Only when the battle has ended. */
  points?: number;
  seed?: number;
  actions?: PlayerAction[];
  /** Only when the battle has ended: the NEAR account named at /battle/start, if any. */
  account?: string;
  /** Only when the battle has ended: the curfew the ending broke (points were 0). */
  curfew?: string;
  /** Only when the battle has ended: where it was fought. */
  ward?: WardId;
}

type Answer<T> = { status: 200; body: T } | { status: number; body: { error: string } };

interface Session {
  id: string;
  ward: WardId;
  by: string;
  account?: string;
  seed: number;
  wildId: string;
  omen: Omen;
  playerCurse: string;
  rng: Rng;
  state: BattleState;
  actions: PlayerAction[];
  lastSeen: number;
}

export interface BattleContext { account?: string; ward: WardId; omen: Omen }
export type BattleGuard = (context: BattleContext, action?: PlayerAction) => string | null;

function guardedView(state: BattleState, context: BattleContext, guard?: BattleGuard): BattleView {
  const view = battleView(state);
  if (!guard) return view;
  const actions: Record<keyof BattleView["canUse"], PlayerAction> = {
    attack: { kind: "attack" }, ward: { kind: "ward" }, seal: { kind: "seal" },
    gildedSeal: { kind: "seal", gilded: true }, salve: { kind: "item", item: "salve" }, flee: { kind: "flee" },
  };
  for (const key of Object.keys(actions) as (keyof typeof actions)[]) {
    if (guard(context, actions[key])) view.canUse[key] = false;
  }
  return view;
}

export interface BattleSessionOptions {
  now?: () => number;
  newSeed?: () => number;
  newId?: () => string;
}

function side(c: Combatant): SideView {
  return { speciesId: c.speciesId, name: c.name, emotion: c.emotion, omen: c.omen, hp: c.hp, maxHp: c.maxHp };
}

export function battleView(s: BattleState): BattleView {
  return {
    turn: s.turn,
    outcome: s.outcome,
    player: side(s.player),
    wild: side(s.wild),
    items: { ...s.items },
    chances: { seal: sealChance(s.wild), gildedSeal: sealChance(s.wild, true), flee: fleeChance(s) },
    canUse: {
      attack: canUse(s, { kind: "attack" }),
      ward: canUse(s, { kind: "ward" }),
      seal: canUse(s, { kind: "seal" }),
      gildedSeal: canUse(s, { kind: "seal", gilded: true }),
      salve: canUse(s, { kind: "item", item: "salve" }),
      flee: canUse(s, { kind: "flee" }),
    },
  };
}

/** Accepts only the exact action shapes the resolver knows. */
export function parseAction(a: unknown): PlayerAction | null {
  if (!a || typeof a !== "object") return null;
  const x = a as Record<string, unknown>;
  switch (x.kind) {
    case "attack": return { kind: "attack" };
    case "ward": return { kind: "ward" };
    case "flee": return { kind: "flee" };
    case "seal":
      if (x.gilded !== undefined && typeof x.gilded !== "boolean") return null;
      return x.gilded ? { kind: "seal", gilded: true } : { kind: "seal" };
    case "item": return x.item === "salve" ? { kind: "item", item: "salve" } : null;
    default: return null;
  }
}

export class BattleSessions {
  private readonly open = new Map<string, Session>();
  private readonly now: () => number;
  private readonly newSeed: () => number;
  private readonly newId: () => string;

  constructor(private readonly world: World, opts: BattleSessionOptions = {}) {
    this.now = opts.now ?? Date.now;
    this.newSeed = opts.newSeed ?? (() => randomInt(0, 0x1_0000_0000));
    this.newId = opts.newId ?? randomUUID;
  }

  get openCount(): number {
    return this.open.size;
  }

  abandonAccount(account: string): void {
    for (const [id, s] of this.open) if (s.account === account) this.open.delete(id);
  }

  start(body: unknown, guard?: BattleGuard): Answer<StartAnswer> {
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    const ward = b.ward;
    if (typeof ward !== "string" || !(WARDS as readonly string[]).includes(ward)) return fail(400, "unknown ward");
    const playerCurse = b.playerCurse === undefined ? DEFAULT_PLAYER_CURSE : b.playerCurse;
    if (typeof playerCurse !== "string" || !isCurse(playerCurse)) return fail(400, "playerCurse must be a curse id");
    const by = typeof b.by === "string" && b.by.trim() ? b.by.trim().slice(0, 40) : "A newcomer";
    if (b.account !== undefined && !isAccountId(b.account)) return fail(400, "account must be a NEAR account id");
    const account = b.account as string | undefined;

    const noExorcise = curfewText(this.world, "exorcise", ward as WardId);
    const noBind = curfewText(this.world, "bind", ward as WardId);
    if (noExorcise && noBind) return fail(409, `Curfew: "${noExorcise}" and "${noBind}". Come back when it lifts.`);

    this.sweep();
    if (this.open.size >= MAX_OPEN_BATTLES) return fail(503, "too many battles in progress, try again shortly");

    const seed = this.newSeed();
    // The encounter roll uses its own stream so the battle itself replays from createRng(seed) alone.
    const wild = rollEncounter(this.world.wards[ward as WardId].grudge, createRng((seed ^ 0x9e3779b9) >>> 0));
    const refusal = guard?.({ account, ward: ward as WardId, omen: wild.omen });
    if (refusal) return fail(409, refusal);
    const state = createBattle({ wild: wild.id, omen: wild.omen, playerCurse });
    const s: Session = {
      id: this.newId(), ward: ward as WardId, by, account, seed, wildId: wild.id, omen: wild.omen, playerCurse,
      rng: createRng(seed), state, actions: [], lastSeen: this.now(),
    };
    this.open.set(s.id, s);

    const m = typeMultiplier(state.player.emotion, state.wild.emotion);
    return ok({
      battleId: s.id,
      ward: s.ward,
      wild: { id: wild.id, name: wild.name, emotion: wild.emotion, omen: wild.omen },
      matchup: m > 1 ? "advantage" : m < 1 ? "disadvantage" : "even",
      view: guardedView(state, s, guard),
      lines: [
        introLine(state),
        ...(noExorcise ? [`Kessa: "Curfew: ${noExorcise}. Seal it, or the Rite pays nothing."`] : []),
        ...(noBind ? [`Kessa: "Curfew: ${noBind}. Drive it off, don't seal it."`] : []),
        ...[...new Set([guard?.(s, { kind: "attack" }), guard?.(s, { kind: "item", item: "salve" })])]
          .filter((line): line is string => !!line).map((line) => `Kessa: "${line}"`),
      ],
      curfews: [noExorcise, noBind].filter((x): x is string => x !== null),
    });
  }

  turn(body: unknown, authenticatedAccount?: string, guard?: BattleGuard): Answer<TurnAnswer> {
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    this.sweep();
    const s = typeof b.battleId === "string" ? this.open.get(b.battleId) : undefined;
    if (!s) return fail(404, "no battle with that id (it ended or expired)");
    if (authenticatedAccount !== undefined && s.account !== authenticatedAccount) return fail(403, "this battle belongs to another player");
    const action = parseAction(b.action);
    if (!action) return fail(400, "unknown action");
    const refusal = guard?.(s, action);
    if (refusal) return fail(409, refusal);
    if (!canUse(s.state, action)) return fail(409, `you can't ${actionLabel(action)} right now`);
    const noBind = action.kind === "seal" ? curfewText(this.world, "bind", s.ward) : null;
    if (noBind) return fail(409, `Curfew: "${noBind}". You can't seal it here now.`);

    const t = resolveTurn(s.state, action, s.rng);
    s.state = t.state;
    s.actions.push(action);
    s.lastSeen = this.now();
    const answer: TurnAnswer = {
      battleId: s.id,
      action: actionLabel(action),
      events: t.events,
      lines: t.events.map((e) => describeEvent(e, t.state)),
      view: guardedView(t.state, s, guard),
      outcome: t.state.outcome,
    };

    const outcome = t.state.outcome ?? (s.actions.length >= MAX_TURNS ? "fled" : null);
    if (outcome) {
      this.open.delete(s.id);
      answer.outcome = outcome;
      const curfew = outcome === "bound" || outcome === "won" ? curfewText(this.world, outcome === "bound" ? "bind" : "exorcise", s.ward) : null;
      if (curfew) {
        answer.curfew = curfew;
        answer.lines.push(`Kessa: "That broke the curfew (${curfew}). The Rite pays nothing for it."`);
      }
      answer.points = outcome === "bound" || outcome === "won"
        ? recordCapture(this.world, {
            ward: s.ward, speciesId: s.wildId, omen: s.omen, verb: outcome === "bound" ? "bind" : "exorcise", by: s.by,
          })
        : 0;
      answer.seed = s.seed;
      answer.actions = s.actions;
      answer.ward = s.ward;
      if (s.account) answer.account = s.account;
    }
    return ok(answer);
  }

  /** Drops battles nobody has touched for BATTLE_IDLE_MS. */
  private sweep() {
    const cutoff = this.now() - BATTLE_IDLE_MS;
    for (const [id, s] of this.open) if (s.lastSeen < cutoff) this.open.delete(id);
  }
}

export interface CurseEntry {
  id: string;
  name: string;
  emotion: Combatant["emotion"];
  omen: Omen;
  home: WardId;
  bornFrom: string;
  signatureMove: { name: string; effect: string };
}

/**
 * GET /curses: names and looks of every curse, for clients without a copy of the game data
 * (the Unreal Talisman Case shows names from it). Stats stay on the server with the battle rules.
 */
export function curseCatalog(): CurseEntry[] {
  return CURSES.map((c) => ({
    id: c.id, name: c.name, emotion: c.emotion, omen: c.omen, home: c.home, bornFrom: c.bornFrom,
    signatureMove: { ...c.signatureMove },
  }));
}

function isCurse(id: string): boolean {
  try {
    curseById(id);
    return true;
  } catch {
    return false;
  }
}

function ok<T>(body: T): Answer<T> {
  return { status: 200, body };
}

function fail(status: number, error: string): { status: number; body: { error: string } } {
  return { status, body: { error } };
}

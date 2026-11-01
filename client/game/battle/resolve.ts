/**
 * Pure, deterministic battle resolution. No Phaser and no Math.random in here: every roll comes
 * from the seeded Rng passed in, so a battle can be replayed exactly from { seed, actions }.
 *
 * Balance numbers are deliberately simple and live in the constants below. Species stats, the
 * type triangle and the base binding chance come from @grudgebound/shared and are not changed here.
 */
import {
  bindChance, createRng, curseById, typeMultiplier,
  type CurseSpecies, type Emotion, type Omen, type Rng,
} from "@grudgebound/shared";

// ------------------------------------------------------------------ tuning constants

/** Raw damage = attack x ATTACK_SCALE - guard x GUARD_SCALE (before type, variance, crit, ward). */
export const ATTACK_SCALE = 1.4;
export const GUARD_SCALE = 0.5;
/** Every hit does at least this much. */
export const MIN_DAMAGE = 1;
/** Each hit rolls a multiplier in [VARIANCE_MIN, 1]. */
export const VARIANCE_MIN = 0.85;
export const CRIT_CHANCE = 1 / 16;
export const CRIT_MULTIPLIER = 1.5;
/** Warding multiplies incoming damage by this for the rest of the turn. Wards always act first. */
export const WARD_DAMAGE_FACTOR = 0.5;
/** Chance the wild curse wards instead of attacking (it never wards twice in a row). */
export const WILD_WARD_CHANCE = 0.2;

/** Flee chance = FLEE_BASE + (your speed - their speed) x FLEE_SPEED_STEP + FLEE_RETRY_BONUS per failed try. */
export const FLEE_BASE = 0.5;
export const FLEE_SPEED_STEP = 0.05;
export const FLEE_RETRY_BONUS = 0.15;
export const FLEE_MIN = 0.1;
export const FLEE_MAX = 0.95;
/** Calamity curses hold you in place: flee chance is multiplied by this. */
export const FLEE_CALAMITY_FACTOR = 0.5;

/** Ink Salve restores this fraction of max HP. */
export const SALVE_HEAL_FRACTION = 0.4;
/** Talisman quality passed to shared bindChance(): plain paper vs gilded. */
export const PLAIN_TALISMAN_QUALITY = 1;
export const GILDED_TALISMAN_QUALITY = 1.5;

export const STARTING_ITEMS: Inventory = { salve: 2, gilded: 1 };

/** Bound curse used when the caller does not pass one (and its fallback if the wild one is the same species). */
export const DEFAULT_PLAYER_CURSE = "cinder-grin";
export const DEFAULT_PLAYER_CURSE_ALT = "lantern-drowned";

// ------------------------------------------------------------------ types

export type Side = "player" | "wild";
export type Outcome = "won" | "fled" | "bound" | "lost";
export type ItemId = "salve" | "gilded";
export type Inventory = Record<ItemId, number>;

/** What the player chose this turn. "flee" is offered from the Item menu as "Slip away". */
export type PlayerAction =
  | { kind: "attack" }
  | { kind: "ward" }
  | { kind: "seal"; gilded?: boolean }
  | { kind: "item"; item: "salve" }
  | { kind: "flee" };

export interface Combatant {
  speciesId: string;
  name: string;
  emotion: Emotion;
  omen: Omen;
  maxHp: number;
  hp: number;
  attack: number;
  guard: number;
  speed: number;
}

export interface BattleState {
  player: Combatant;
  wild: Combatant;
  items: Inventory;
  turn: number;
  fleeAttempts: number;
  wildWardedLastTurn: boolean;
  outcome: Outcome | null;
}

export type BattleEvent =
  | { kind: "attack"; by: Side; damage: number; multiplier: number; crit: boolean; warded: boolean; hpAfter: number }
  | { kind: "ward"; by: Side }
  | { kind: "heal"; by: Side; amount: number; hpAfter: number }
  | { kind: "seal"; chance: number; gilded: boolean; success: boolean }
  | { kind: "flee"; chance: number; success: boolean }
  | { kind: "faint"; who: Side }
  | { kind: "end"; outcome: Outcome };

export interface TurnResult {
  state: BattleState;
  events: BattleEvent[];
}

// ------------------------------------------------------------------ setup

export function makeCombatant(species: CurseSpecies, omen: Omen = species.omen): Combatant {
  const s = species.stats;
  return {
    speciesId: species.id, name: species.name, emotion: species.emotion, omen,
    maxHp: s.hp, hp: s.hp, attack: s.attack, guard: s.guard, speed: s.speed,
  };
}

export function defaultPlayerCurse(wildId: string): string {
  return wildId === DEFAULT_PLAYER_CURSE ? DEFAULT_PLAYER_CURSE_ALT : DEFAULT_PLAYER_CURSE;
}

export interface BattleSetup {
  wild: string;
  omen?: Omen;
  playerCurse?: string;
  items?: Inventory;
}

export function createBattle(setup: BattleSetup): BattleState {
  const wild = curseById(setup.wild);
  const player = curseById(setup.playerCurse ?? defaultPlayerCurse(setup.wild));
  return {
    player: makeCombatant(player),
    wild: makeCombatant(wild, setup.omen ?? wild.omen),
    items: { ...(setup.items ?? STARTING_ITEMS) },
    turn: 0,
    fleeAttempts: 0,
    wildWardedLastTurn: false,
    outcome: null,
  };
}

// ------------------------------------------------------------------ formulas

export interface DamageRoll {
  damage: number;
  multiplier: number;
  crit: boolean;
}

/** Rolls one hit. Always consumes exactly two random numbers (variance, crit). */
export function rollDamage(attacker: Combatant, defender: Combatant, rng: Rng, warded = false): DamageRoll {
  const raw = Math.max(MIN_DAMAGE, attacker.attack * ATTACK_SCALE - defender.guard * GUARD_SCALE);
  const multiplier = typeMultiplier(attacker.emotion, defender.emotion);
  const variance = VARIANCE_MIN + rng.next() * (1 - VARIANCE_MIN);
  const crit = rng.next() < CRIT_CHANCE;
  const scaled = raw * multiplier * variance * (crit ? CRIT_MULTIPLIER : 1) * (warded ? WARD_DAMAGE_FACTOR : 1);
  return { damage: Math.max(MIN_DAMAGE, Math.round(scaled)), multiplier, crit };
}

/** Uses shared bindChance(): base(omen) x (1 - HP%) x talisman quality, capped at 0.95. */
export function sealChance(wild: Combatant, gilded = false): number {
  return bindChance(wild.omen, wild.hp / wild.maxHp, gilded ? GILDED_TALISMAN_QUALITY : PLAIN_TALISMAN_QUALITY);
}

export function fleeChance(state: BattleState): number {
  const { player, wild } = state;
  let p = FLEE_BASE + (player.speed - wild.speed) * FLEE_SPEED_STEP + state.fleeAttempts * FLEE_RETRY_BONUS;
  if (wild.omen === "calamity") p *= FLEE_CALAMITY_FACTOR;
  return Math.min(FLEE_MAX, Math.max(FLEE_MIN, p));
}

// ------------------------------------------------------------------ turn resolution

function cloneState(s: BattleState): BattleState {
  return { ...s, player: { ...s.player }, wild: { ...s.wild }, items: { ...s.items } };
}

/** Is the action allowed right now? (The UI greys out the rest.) */
export function canUse(state: BattleState, action: PlayerAction): boolean {
  if (state.outcome) return false;
  if (action.kind === "item") return state.items[action.item] > 0;
  if (action.kind === "seal" && action.gilded) return state.items.gilded > 0;
  return true;
}

/**
 * Resolves one full turn: the player's action, then the wild curse's reply.
 * Order: wards first; then a seal / item / flee attempt (instant); then attacks by Speed
 * (ties go to the player). Returns a new state; the input is not modified.
 */
export function resolveTurn(prev: BattleState, action: PlayerAction, rng: Rng): TurnResult {
  if (prev.outcome) throw new Error("battle is already over");
  if (!canUse(prev, action)) throw new Error(`action not available: ${JSON.stringify(action)}`);
  const s = cloneState(prev);
  const events: BattleEvent[] = [];
  s.turn += 1;

  // Wild curse decides first (one roll), so the roll order never depends on the player's choice.
  const wildWards = !s.wildWardedLastTurn && rng.chance(WILD_WARD_CHANCE);
  s.wildWardedLastTurn = wildWards;
  const playerWards = action.kind === "ward";

  const end = (outcome: Outcome) => {
    s.outcome = outcome;
    events.push({ kind: "end", outcome });
  };

  // 1. Wards (priority).
  if (playerWards) events.push({ kind: "ward", by: "player" });
  if (wildWards) events.push({ kind: "ward", by: "wild" });

  // 2. Instant player actions.
  if (action.kind === "seal") {
    const gilded = !!action.gilded;
    if (gilded) s.items.gilded -= 1;
    const chance = sealChance(s.wild, gilded);
    const success = rng.chance(chance);
    events.push({ kind: "seal", chance, gilded, success });
    if (success) {
      end("bound");
      return { state: s, events };
    }
  } else if (action.kind === "item") {
    s.items.salve -= 1;
    const before = s.player.hp;
    s.player.hp = Math.min(s.player.maxHp, s.player.hp + Math.round(s.player.maxHp * SALVE_HEAL_FRACTION));
    events.push({ kind: "heal", by: "player", amount: s.player.hp - before, hpAfter: s.player.hp });
  } else if (action.kind === "flee") {
    const chance = fleeChance(s);
    const success = rng.chance(chance);
    events.push({ kind: "flee", chance, success });
    if (success) {
      end("fled");
      return { state: s, events };
    }
    s.fleeAttempts += 1;
  }

  // 3. Attacks, fastest first.
  const attackers: Side[] = [];
  if (action.kind === "attack") attackers.push("player");
  if (!wildWards) attackers.push("wild");
  if (attackers.length === 2 && s.wild.speed > s.player.speed) attackers.reverse();

  for (const by of attackers) {
    const attacker = by === "player" ? s.player : s.wild;
    const defender = by === "player" ? s.wild : s.player;
    const warded = by === "player" ? wildWards : playerWards;
    const roll = rollDamage(attacker, defender, rng, warded);
    defender.hp = Math.max(0, defender.hp - roll.damage);
    events.push({ kind: "attack", by, ...roll, warded, hpAfter: defender.hp });
    if (defender.hp === 0) {
      const who: Side = by === "player" ? "wild" : "player";
      events.push({ kind: "faint", who });
      end(who === "wild" ? "won" : "lost");
      break;
    }
  }
  return { state: s, events };
}

/** Replays a whole battle from its seed and the player's actions. Same input, same result. */
export function replayBattle(setup: BattleSetup, seed: number, actions: readonly PlayerAction[]): TurnResult[] {
  const rng = createRng(seed);
  let state = createBattle(setup);
  const turns: TurnResult[] = [];
  for (const a of actions) {
    if (state.outcome) break;
    const t = resolveTurn(state, a, rng);
    turns.push(t);
    state = t.state;
  }
  return turns;
}

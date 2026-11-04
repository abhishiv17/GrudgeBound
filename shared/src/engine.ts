/**
 * The tick engine: one tick = one in-game hour.
 * Shared by the game server (real time) and the simulator (fast-forward), so both
 * produce exactly the same world for the same seed.
 *
 * Status: first playable economy model. Battles are resolved with simple odds here;
 * the full turn-based battle system arrives on Days 3-6.
 */
import { EMOTIONS, type Emotion } from "./emotions";
import { WARDS, WARD_INFO, type WardId } from "./wards";
import { CURSES, curseById, cursesWhere, bindChance, type CurseSpecies, type Omen } from "./curses";
import { createRng, type Rng } from "./rng";
import { TICKS_PER_DAY, dayOf, hourOf, isNight } from "./time";
import {
  type GrudgeMix, emptyGrudge, totalGrudge, addGrudge, removeGrudge, decayTick,
  rollSpawnOmen, rollSpawnEmotion, GRUDGE_SOURCES, GRUDGE_SINKS, DAILY_HUNT_QUOTA, CALAMITY_THRESHOLD,
} from "./grudge";
import {
  type MarketItem, quoteSellPrice, quoteBuyPrice, recordSale, recordPurchase, rollDay, BANKRUPT_EMBER, BANKRUPT_DAYS,
} from "./pricing";
import { POINTS_FOR_OMEN, pvpTake } from "./points";
import { type Rule, type RuleParams, ruleCost, arbiterCheck, renderRule, wardsTouched, isCruel } from "./rules";

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

export interface WardState {
  id: WardId;
  grudge: GrudgeMix;
  /** Species ids of curses currently roaming this Ward. */
  wild: string[];
  bindingsToday: Record<Emotion, number>;
  calamityRaisedToday: boolean;
}

export interface Merchant {
  id: string;
  name: string;
  role: "broker" | "apothecary" | "fence";
  ward: WardId;
  ember: number;
  closed: boolean;
  lowDays: number;
  items: MarketItem[];
}

export type Personality = "hoarder" | "zealot" | "schemer" | "mercenary" | "gambler" | "player";

export interface Participant {
  id: string;
  name: string;
  kind: "human" | "agent";
  personality: Personality;
  ward: WardId;
  points: number;
  ember: number;
  /** Species ids of bound curses (talismans). */
  talismans: string[];
  /** Day of this participant's last rejected proposal: one public challenge a day, no spam. */
  rejectedDay?: number;
}

export interface GameEvent {
  tick: number;
  day: number;
  hour: number;
  type: string;
  ward?: WardId;
  actor?: string;
  text: string;
  /** A link for the event (e.g. the NEAR transaction behind a "chain" event). */
  url?: string;
}

export interface World {
  seed: number;
  tick: number;
  wards: Record<WardId, WardState>;
  merchants: Merchant[];
  participants: Participant[];
  rules: Rule[];
  nextRuleId: number;
  burnedEmber: number;
  events: GameEvent[];
}

// ---------------------------------------------------------------------------
// Tunables (AI & Economy seat: tune these with `npm run sim`)
// ---------------------------------------------------------------------------

/**
 * The world seed the demo runs on (P2-A1). Over 7 days: the Fence goes bankrupt (day 3), a Calamity rises in
 * the same Ward (day 4), and a mercenary challenges a PvP ban, which Kessa rejects (day 5): see npm run sim:seeds.
 */
export const DEMO_SEED = 2;

export const TUNING = {
  /** Background emotion from ordinary town life, per tick, toward each Ward's leaning. */
  ambientGrudgePerTick: 0.4,
  spawnChancePerTick: 0.6,
  maxWildPerWard: 8,
  /** Chance a participant acts in a given tick. */
  actChance: 0.7,
  huntSuccess: { whisper: 0.85, wail: 0.55, calamity: 0.25 } as Record<Omen, number>,
  /** HP fraction left when a won fight turns into a binding attempt. */
  bindAtHpFraction: 0.25,
  merchantUpkeepPerTick: 4,
  townsfolkBuyChance: 0.25,
  restockCostRatio: 0.5,
  scamChance: 0.3,
  investAmount: 100,
  proposeChance: 0.15,
  maxEvents: 2000,
} as const;

// ---------------------------------------------------------------------------
// World creation
// ---------------------------------------------------------------------------

const item = (id: string, name: string, base: number, stock: number, protective: boolean): MarketItem => ({
  id, name, base, stock, targetStock: Math.max(stock, 1), buys24h: 0, sells24h: 0, protective,
});

const talismanItems = (stock: number): MarketItem[] =>
  EMOTIONS.map((e) => item(`talisman-${e}`, `${e[0]!.toUpperCase()}${e.slice(1)} talismans`, 30, stock, false));

export function createWorld(seed: number): World {
  const wards = {} as Record<WardId, WardState>;
  for (const id of WARDS) {
    wards[id] = { id, grudge: emptyGrudge(), wild: [], bindingsToday: { rage: 0, sorrow: 0, dread: 0 }, calamityRaisedToday: false };
  }

  const merchants: Merchant[] = [
    { id: "broker", name: "the Talisman Broker", role: "broker", ward: "lantern", ember: 400, closed: false, lowDays: 0,
      items: [...talismanItems(6), item("binding-seal", "Binding seals", 15, 10, true)] },
    { id: "apothecary", name: "the Apothecary", role: "apothecary", ward: "ash", ember: 300, closed: false, lowDays: 0,
      items: [item("salve", "Salves", 10, 12, false), item("ward-charm", "Ward charms", 20, 8, true), item("binding-seal", "Binding seals", 15, 8, true)] },
    { id: "fence", name: "the Fence", role: "fence", ward: "tide", ember: 160, closed: false, lowDays: 0,
      items: [...talismanItems(4), item("salve", "Salves", 10, 6, false)] },
  ];

  const p = (id: string, name: string, kind: Participant["kind"], personality: Personality, ward: WardId): Participant => ({
    id, name, kind, personality, ward, points: 0, ember: 60, talismans: [],
  });

  const participants: Participant[] = [
    p("fen", "Old Mother Fen", "agent", "hoarder", "lantern"),
    p("vane", "Brother Vane", "agent", "zealot", "bone"),
    p("silk", "Silk", "agent", "schemer", "tide"),
    p("rook-ash", "Rook & Ash", "agent", "mercenary", "ash"),
    p("pell", "Pell", "agent", "gambler", "lantern"),
    // Stand-ins for human players in the simulator.
    p("player-1", "Player 1", "human", "player", "lantern"),
    p("player-2", "Player 2", "human", "player", "ash"),
    p("player-3", "Player 3", "human", "player", "tide"),
  ];

  return { seed, tick: 0, wards, merchants, participants, rules: [], nextRuleId: 1, burnedEmber: 0, events: [] };
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

export type ParticipantAction =
  | { type: "hunt" }
  | { type: "sell"; merchantId: string; speciesId: string }
  | { type: "buy"; merchantId: string; itemId: string }
  | { type: "pvp"; targetId: string }
  | { type: "move"; ward: WardId }
  | { type: "proposeRule"; templateId: string; params: RuleParams }
  | { type: "invest"; merchantId: string }
  | { type: "idle" };

export type Policy = (world: World, me: Participant, options: ParticipantAction[], rng: Rng) => ParticipantAction;

function activeRules(world: World, templateId: string): Rule[] {
  return world.rules.filter((r) => r.templateId === templateId);
}

/** The parts of a rule the helpers below need: works on World rules and on WorldSummary rules alike. */
export type RuleLike = Pick<Rule, "templateId" | "params">;

/** The curfew (R2) in `rules` that forbids `action` in `ward` at `tick`, if any. */
export function curfewRule<R extends RuleLike>(rules: readonly R[], action: string, ward: WardId, tick: number): R | undefined {
  return rules.find(
    (r) => r.templateId === "R2" && r.params.action === action && r.params.ward === ward && (r.params.time === "night") === isNight(tick),
  );
}

/** Is `action` forbidden in `ward` at `tick` by a curfew (R2) in `rules`? */
export function curfewForbids(rules: readonly RuleLike[], action: string, ward: WardId, tick: number): boolean {
  return curfewRule(rules, action, ward, tick) !== undefined;
}

/** "Kessa's curfew text" for the rule forbidding `action` in `ward` right now, or null. */
export function curfewText(world: World, action: string, ward: WardId): string | null {
  const r = curfewRule(world.rules, action, ward, world.tick);
  return r ? renderRule(r.templateId, r.params) : null;
}

/** Points multiplier from bonus rules (R1: "[action] in [Ward] gives x[m] points") in `rules`. */
export function ruleMultiplier(rules: readonly RuleLike[], action: string, ward: WardId): number {
  return rules
    .filter((r) => r.templateId === "R1" && r.params.action === action && r.params.ward === ward)
    .reduce((m, r) => m * Number(r.params.multiplier), 1);
}

/** Points for binding or exorcising a curse of `omen` in `ward`, with the rules in force applied. */
export function capturePoints(rules: readonly RuleLike[], verb: CaptureVerb, ward: WardId, omen: Omen): number {
  return Math.round(POINTS_FOR_OMEN[omen] * ruleMultiplier(rules, verb, ward));
}

function forbidden(world: World, action: string, ward: WardId): boolean {
  return curfewForbids(world.rules, action, ward, world.tick);
}

function pointsMultiplier(world: World, action: string, ward: WardId): number {
  return ruleMultiplier(world.rules, action, ward);
}

/** Every action the rules allow this participant to take now. Agents may only choose from this list. */
export function legalActions(world: World, me: Participant): ParticipantAction[] {
  const out: ParticipantAction[] = [{ type: "idle" }];
  const ward = world.wards[me.ward];
  if (ward.wild.length > 0 && !forbidden(world, "exorcise", me.ward)) out.push({ type: "hunt" });

  const shops = world.merchants.filter((m) => m.ward === me.ward && !m.closed);
  if (!forbidden(world, "trade", me.ward)) {
    for (const m of shops) {
      for (const sp of new Set(me.talismans)) {
        const emotion = curseById(sp).emotion;
        if (m.items.some((i) => i.id === `talisman-${emotion}`)) out.push({ type: "sell", merchantId: m.id, speciesId: sp });
      }
      for (const i of m.items) if (i.stock > 0) out.push({ type: "buy", merchantId: m.id, itemId: i.id });
    }
  }

  const pvpBanned = activeRules(world, "R8").some((r) => r.params.ward === me.ward) || forbidden(world, "pvp", me.ward);
  if (!pvpBanned) {
    for (const o of world.participants) if (o.id !== me.id && o.ward === me.ward && o.points > 0) out.push({ type: "pvp", targetId: o.id });
  }

  for (const w of WARDS) if (w !== me.ward) out.push({ type: "move", ward: w });

  for (const m of world.merchants) {
    if (m.closed && m.ward === me.ward && me.ember >= TUNING.investAmount) out.push({ type: "invest", merchantId: m.id });
  }
  return out;
}

// ---------------------------------------------------------------------------
// Scripted policy (used in fast-forward, and as the fallback when the AI is unavailable)
// ---------------------------------------------------------------------------

const WEIGHTS: Record<Personality, Partial<Record<ParticipantAction["type"], number>>> = {
  hoarder: { hunt: 3, buy: 3, sell: 0.5, move: 0.3, idle: 1 },
  zealot: { hunt: 6, move: 0.6, idle: 0.5 },
  schemer: { sell: 4, hunt: 2, move: 0.5, idle: 1 },
  mercenary: { pvp: 4, hunt: 2, move: 0.6, idle: 0.5 },
  gambler: { invest: 6, hunt: 2, move: 1.5, buy: 0.5, idle: 0.5 },
  player: { hunt: 4, sell: 2, pvp: 0.6, move: 0.8, buy: 0.3, invest: 2, idle: 1 },
};

/** The rule each personality wants, given the world. */
export function desiredRule(world: World, me: Participant, rng: Rng): { templateId: string; params: RuleParams } | null {
  switch (me.personality) {
    case "hoarder": return { templateId: "R7", params: { percent: 5, payee: "author" } };
    case "schemer": return rng.chance(0.5)
      ? { templateId: "R3", params: { ward: me.ward, amount: 5, payee: "author" } }
      : { templateId: "R7", params: { percent: 8, payee: "author" } };
    case "zealot": {
      // The rule war: where someone pays for PvP, the zealot moves to ban it.
      const rewarded = world.rules.filter((r) => r.templateId === "R1" && r.params.action === "pvp");
      if (rewarded.length && rng.chance(0.6)) return { templateId: "R8", params: { ward: String(rng.pick(rewarded).params.ward) } };
      return { templateId: "R8", params: { ward: me.ward } };
    }
    case "mercenary": {
      // ...and where PvP is banned, the mercenary demands double points for it.
      const banned = world.rules.filter((r) => r.templateId === "R8");
      if (banned.length && rng.chance(0.6)) return { templateId: "R1", params: { action: "pvp", ward: String(rng.pick(banned).params.ward), multiplier: "2" } };
      return rng.chance(0.5)
        ? { templateId: "R1", params: { action: "pvp", ward: me.ward, multiplier: "2" } }
        : { templateId: "R5", params: { target: "the points leader", amount: 20 } };
    }
    case "gambler": return { templateId: "R1", params: { action: "bind", ward: rng.pick(WARDS), multiplier: "2" } };
    case "player": return rng.chance(0.5)
      ? { templateId: "R8", params: { ward: "lantern" } }
      : { templateId: "R1", params: { action: "exorcise", ward: me.ward, multiplier: "1.5" } };
  }
}

export const scriptedPolicy: Policy = (world, me, options, rng) => {
  if (me.points >= ruleCost(world.rules.length) && rng.chance(TUNING.proposeChance)) {
    const want = desiredRule(world, me, rng);
    const verdict = want ? arbiterCheck(want.templateId, want.params, world.rules) : null;
    if (want && verdict?.accepted) return { type: "proposeRule", ...want };
    // A rule that contradicts one in force is a public challenge (Kessa rejects it, at no cost): at most one a day
    // per participant. Duplicates and rules over the cap are never proposed.
    if (want && verdict && !verdict.accepted && verdict.reason.startsWith("Contradicts") && me.rejectedDay !== dayOf(world.tick)) {
      return { type: "proposeRule", ...want };
    }
  }
  const w = WEIGHTS[me.personality];
  return rng.weighted(options, (o) => {
    let weight = w[o.type] ?? 0;
    if (o.type === "buy") {
      const m = world.merchants.find((x) => x.id === o.merchantId);
      const it = m?.items.find((i) => i.id === o.itemId);
      // Hoarders only buy talismans when they're cheap.
      if (!it || !it.id.startsWith("talisman") || me.ember < it.base) weight = 0;
    }
    if (o.type === "pvp") {
      const t = world.participants.find((x) => x.id === o.targetId);
      weight *= t && t.points > me.points ? 1.5 : 0.5;
    }
    return weight;
  });
};

// ---------------------------------------------------------------------------
// The tick
// ---------------------------------------------------------------------------

/** Adds a line to the town log (Kessa's banner shows the newest). */
export function log(world: World, e: Omit<GameEvent, "tick" | "day" | "hour">) {
  world.events.push({ tick: world.tick, day: dayOf(world.tick), hour: hourOf(world.tick), ...e });
  if (world.events.length > TUNING.maxEvents) world.events.splice(0, world.events.length - TUNING.maxEvents);
}

/** A curse leaves the Ward: its emotion drains from the Grudge (over-hunting one emotion feeds it back). */
function drainCapture(world: World, wardId: WardId, sp: CurseSpecies) {
  const ward = world.wards[wardId];
  ward.grudge = removeGrudge(ward.grudge, GRUDGE_SINKS.exorcise[sp.omen], sp.emotion);
  ward.bindingsToday[sp.emotion] += 1;
  if (ward.bindingsToday[sp.emotion] > DAILY_HUNT_QUOTA) ward.grudge = addGrudge(ward.grudge, sp.emotion, 1);
}

export type CaptureVerb = "bind" | "exorcise";

export interface CaptureReport {
  ward: WardId;
  speciesId: string;
  omen: Omen;
  verb: CaptureVerb;
  /** Display name of whoever fought it. */
  by: string;
}

/**
 * A battle outside the tick loop (a real player's fight) ended in a binding or an exorcism:
 * drain the Ward's Grudge like any hunt and tell the town. Returns the points it earned under the rules in force.
 * The caller decides who gets the points (the client's player store now, the Rite contract later).
 */
export function recordCapture(world: World, c: CaptureReport): number {
  const sp = curseById(c.speciesId);
  const curfew = curfewText(world, c.verb, c.ward);
  drainCapture(world, c.ward, sp);
  if (curfew) {
    // Players can still fight under a curfew; the Rite just won't pay for what it forbids.
    log(world, {
      type: c.verb, ward: c.ward, actor: c.by,
      text: `${c.by} ${c.verb === "bind" ? "bound" : "exorcised"} ${sp.name} in ${wardName(c.ward)} against the curfew ("${curfew}"): the Rite pays nothing.`,
    });
    return 0;
  }
  const points = capturePoints(world.rules, c.verb, c.ward, c.omen);
  log(world, {
    type: c.verb, ward: c.ward, actor: c.by,
    text: `${c.by} ${c.verb === "bind" ? "bound" : "exorcised"} ${sp.name} in ${wardName(c.ward)} (+${points} points).`,
  });
  return points;
}

function wardName(id: WardId) {
  return WARD_INFO[id].name;
}

function spawn(world: World, w: WardState, species: CurseSpecies, announce: boolean) {
  w.wild.push(species.id);
  if (announce) log(world, { type: "calamity", ward: w.id, text: `Kessa: "A Calamity rises in ${wardName(w.id)}: ${species.name}!"` });
}

function stepWards(world: World, rng: Rng) {
  const cruelRules = world.rules.filter((r) => isCruel(r.templateId, r.params)).length;
  for (const id of WARDS) {
    const w = world.wards[id];
    w.grudge = decayTick(w.grudge);
    for (const e of WARD_INFO[id].leaning) w.grudge = addGrudge(w.grudge, e, TUNING.ambientGrudgePerTick);

    if (w.wild.length < TUNING.maxWildPerWard && rng.chance(TUNING.spawnChancePerTick)) {
      const omen = rollSpawnOmen(totalGrudge(w.grudge), rng);
      const emotion = rollSpawnEmotion(w.grudge, rng);
      spawn(world, w, rng.pick(cursesWhere(emotion, omen)), false);
    }

    const calamityAllowed = activeRules(world, "R11").every((r) => r.params.ward === id);
    if (!w.calamityRaisedToday && calamityAllowed) {
      if (id === "bone" && cruelRules >= 5) {
        spawn(world, w, curseById("curfew-king"), true);
        w.calamityRaisedToday = true;
      } else if (totalGrudge(w.grudge) >= CALAMITY_THRESHOLD) {
        const dominant = EMOTIONS.reduce((a, b) => (w.grudge[a] >= w.grudge[b] ? a : b));
        spawn(world, w, rng.pick(cursesWhere(dominant, "calamity")), true);
        w.calamityRaisedToday = true;
      }
    }
  }
}

function stepMerchants(world: World, rng: Rng) {
  for (const m of world.merchants) {
    if (m.closed) continue;
    m.ember = Math.max(0, m.ember - TUNING.merchantUpkeepPerTick);
    const fear = totalGrudge(world.wards[m.ward].grudge);
    // Ordinary townsfolk buy salves and charms; fear makes them buy more.
    const goods = m.items.filter((i) => !i.id.startsWith("talisman") && i.stock > 0);
    if (goods.length && rng.chance(TUNING.townsfolkBuyChance + 0.004 * fear)) {
      const idx = m.items.indexOf(rng.pick(goods));
      const it = m.items[idx]!;
      const q = quoteSellPrice(it, { wardGrudge: fear, buyerIsOathbreaker: false });
      m.ember += q.price;
      m.items[idx] = recordSale(it);
    }
  }
}

function act(world: World, me: Participant, action: ParticipantAction, rng: Rng) {
  const ward = world.wards[me.ward];
  switch (action.type) {
    case "idle":
      return;

    case "hunt": {
      const i = rng.int(0, ward.wild.length - 1);
      const sp = curseById(ward.wild[i]!);
      const odds = Math.min(0.95, TUNING.huntSuccess[sp.omen] + 0.02 * Math.min(me.talismans.length, 7));
      if (!rng.chance(odds)) {
        if (sp.omen !== "whisper") log(world, { type: "hunt-failed", ward: me.ward, actor: me.id, text: `${me.name} was driven off by ${sp.name}.` });
        return;
      }
      ward.wild.splice(i, 1);
      const bound = rng.chance(bindChance(sp.omen, TUNING.bindAtHpFraction));
      const verb = bound ? "bind" : "exorcise";
      const pts = Math.round(POINTS_FOR_OMEN[sp.omen] * pointsMultiplier(world, verb, me.ward));
      me.points += pts;
      if (bound) me.talismans.push(sp.id);
      drainCapture(world, me.ward, sp);
      if (sp.omen !== "whisper") {
        log(world, { type: verb, ward: me.ward, actor: me.id, text: `${me.name} ${bound ? "bound" : "exorcised"} ${sp.name} (+${pts} points).` });
      }
      return;
    }

    case "sell": {
      const m = world.merchants.find((x) => x.id === action.merchantId);
      const at = me.talismans.indexOf(action.speciesId);
      if (!m || m.closed || at < 0) return;
      const emotion = curseById(action.speciesId).emotion;
      const idx = m.items.findIndex((i) => i.id === `talisman-${emotion}`);
      if (idx < 0) return;
      const fear = totalGrudge(ward.grudge);
      const q = quoteBuyPrice(m.items[idx]!, { wardGrudge: fear, buyerIsOathbreaker: false });
      if (m.ember < q.price) return;
      m.ember -= q.price;
      m.items[idx] = recordPurchase(m.items[idx]!);
      me.talismans.splice(at, 1);
      me.ember += payTradeTax(world, q.price);
      if (me.personality === "schemer" && rng.chance(TUNING.scamChance)) {
        ward.grudge = addGrudge(ward.grudge, GRUDGE_SOURCES.scam.emotion, GRUDGE_SOURCES.scam.amount);
        log(world, { type: "scam", ward: me.ward, actor: me.id, text: `${me.name} passed a forged talisman to ${m.name}. Rage stirs in ${wardName(me.ward)}.` });
      }
      return;
    }

    case "buy": {
      const m = world.merchants.find((x) => x.id === action.merchantId);
      if (!m || m.closed) return;
      const idx = m.items.findIndex((i) => i.id === action.itemId);
      const it = m.items[idx];
      if (!it || it.stock <= 0) return;
      const q = quoteSellPrice(it, { wardGrudge: totalGrudge(ward.grudge), buyerIsOathbreaker: false });
      if (me.ember < q.price) return;
      me.ember -= q.price;
      m.ember += q.price;
      m.items[idx] = recordSale(it);
      if (it.id.startsWith("talisman-")) {
        const emotion = it.id.slice("talisman-".length) as Emotion;
        const pool = CURSES.filter((c) => c.emotion === emotion && c.omen === "whisper");
        me.talismans.push(rng.pick(pool).id);
      }
      return;
    }

    case "pvp": {
      const t = world.participants.find((x) => x.id === action.targetId);
      if (!t || t.ward !== me.ward) return;
      const myPower = 1 + me.talismans.length;
      const theirPower = 1 + t.talismans.length;
      const iWin = rng.chance(myPower / (myPower + theirPower));
      const [winner, loser] = iWin ? [me, t] : [t, me];
      const take = pvpTake(loser.points);
      const leader = [...world.participants].sort((a, b) => b.points - a.points)[0];
      const bonus = Math.round(take * (pointsMultiplier(world, "pvp", me.ward) - 1)) +
        activeRules(world, "R5").filter((r) => r.params.target === "the points leader" && loser.id === leader?.id)
          .reduce((s, r) => s + Number(r.params.amount), 0);
      loser.points -= take;
      winner.points += take + bonus;
      ward.grudge = addGrudge(ward.grudge, GRUDGE_SOURCES.pvpWin.emotion, GRUDGE_SOURCES.pvpWin.amount);
      if (take + bonus >= 10) log(world, { type: "pvp", ward: me.ward, actor: winner.id, text: `${winner.name} beat ${loser.name} and took ${take + bonus} points.` });
      return;
    }

    case "move": {
      for (const r of activeRules(world, "R3").filter((x) => x.params.ward === action.ward)) {
        const toll = Math.min(me.points, Number(r.params.amount));
        me.points -= toll;
        const author = world.participants.find((x) => x.id === r.author);
        if (r.params.payee === "author" && author && author.id !== me.id) author.points += toll;
      }
      me.ward = action.ward;
      return;
    }

    case "invest": {
      const m = world.merchants.find((x) => x.id === action.merchantId);
      if (!m || !m.closed || me.ember < TUNING.investAmount) return;
      me.ember -= TUNING.investAmount;
      m.ember += TUNING.investAmount;
      m.closed = false;
      m.lowDays = 0;
      const w = world.wards[m.ward];
      w.grudge = removeGrudge(w.grudge, GRUDGE_SINKS.shopReopened, "sorrow");
      log(world, { type: "reopen", ward: m.ward, actor: me.id, text: `${me.name} invested ${TUNING.investAmount} Ember and ${m.name} reopened. Sorrow eases.` });
      return;
    }

    case "proposeRule":
      proposeRule(world, me, action.templateId, action.params);
      return;
  }
}

/** Applies R7 trade taxes to a sale; returns what the seller keeps. */
/** Applies trade-tax rules (R7) to an Ember payment; returns what the seller keeps. */
export function payTradeTax(world: World, amount: number): number {
  let kept = amount;
  for (const r of activeRules(world, "R7")) {
    const tax = Math.floor((amount * Number(r.params.percent)) / 100);
    kept -= tax;
    const author = world.participants.find((x) => x.id === r.author);
    if (r.params.payee === "author" && author) author.ember += tax;
    else world.burnedEmber += tax;
  }
  return Math.max(0, kept);
}

/** Spend points to add a rule. The Arbiter's checks are deterministic; rejected proposals cost nothing. */
export function proposeRule(world: World, me: Participant, templateId: string, params: RuleParams): boolean {
  const cost = ruleCost(world.rules.length);
  if (me.points < cost) return false;
  const verdict = arbiterCheck(templateId, params, world.rules);
  const text = renderRule(templateId, params);
  if (!verdict.accepted) {
    me.rejectedDay = dayOf(world.tick);
    log(world, { type: "rule-rejected", actor: me.id, text: `Kessa rejects ${me.name}'s rule "${text}": ${verdict.reason}` });
    return false;
  }
  me.points -= cost;
  enactRule(world, { author: me.id, authorName: me.name, templateId, params, cost });
  return true;
}

/**
 * Puts a rule the Arbiter accepted into force: Kessa announces it, and a cruel rule angers the Wards it touches.
 * Used by proposeRule and by the server for rules passed on the rite contract (whose points were paid on chain).
 */
export function enactRule(
  world: World,
  r: { author: string; authorName: string; templateId: string; params: RuleParams; cost: number },
): Rule {
  const rule: Rule = { id: world.nextRuleId++, templateId: r.templateId, params: r.params, author: r.author, cost: r.cost, addedAtTick: world.tick };
  world.rules.push(rule);
  const text = renderRule(r.templateId, r.params);
  log(world, { type: "rule-added", actor: r.author, text: `Kessa: "${r.authorName} spent ${r.cost} points. New rule #${rule.id}: ${text}."` });
  if (isCruel(r.templateId, r.params)) {
    for (const w of wardsTouched(r.params)) {
      world.wards[w].grudge = addGrudge(world.wards[w].grudge, GRUDGE_SOURCES.cruelRulePassed.emotion, GRUDGE_SOURCES.cruelRulePassed.amount);
    }
  }
  return rule;
}

/**
 * Takes a rule out of force (the server calls it once the rite contract has repealed it; the repealer paid there).
 * Kessa announces it, and repealing a cruel rule eases the dread it caused in the Wards it touched.
 */
export function repealRule(world: World, ruleId: number, r: { by: string; byName: string; cost: number }): Rule | null {
  const i = world.rules.findIndex((x) => x.id === ruleId);
  if (i < 0) return null;
  const rule = world.rules.splice(i, 1)[0]!;
  const text = renderRule(rule.templateId, rule.params);
  log(world, { type: "rule-repealed", actor: r.by, text: `Kessa: "${r.byName} paid ${r.cost} points. Rule #${rule.id} is struck from the Rite: ${text}."` });
  if (isCruel(rule.templateId, rule.params)) {
    for (const w of wardsTouched(rule.params)) {
      world.wards[w].grudge = removeGrudge(world.wards[w].grudge, GRUDGE_SINKS.cruelRuleRepealed, GRUDGE_SOURCES.cruelRulePassed.emotion);
    }
  }
  return rule;
}

function endOfDay(world: World) {
  for (const m of world.merchants) {
    m.items = m.items.map(rollDay);
    if (m.closed) continue;
    // Restock goods they make themselves (not talismans).
    m.items = m.items.map((i) => {
      if (i.id.startsWith("talisman") || i.stock >= i.targetStock) return i;
      const need = i.targetStock - i.stock;
      const cost = Math.ceil(need * i.base * TUNING.restockCostRatio);
      if (m.ember < cost) return i;
      m.ember -= cost;
      return { ...i, stock: i.targetStock };
    });
    m.lowDays = m.ember < BANKRUPT_EMBER ? m.lowDays + 1 : 0;
    if (m.lowDays >= BANKRUPT_DAYS) {
      m.closed = true;
      const w = world.wards[m.ward];
      w.grudge = addGrudge(w.grudge, GRUDGE_SOURCES.bankruptcy.emotion, GRUDGE_SOURCES.bankruptcy.amount);
      w.wild.push("the-shuttered");
      log(world, { type: "bankruptcy", ward: m.ward, text: `${m.name} went bankrupt. The Shuttered claws out of the shop in ${wardName(m.ward)}!` });
    }
  }
  for (const id of WARDS) {
    const w = world.wards[id];
    w.bindingsToday = { rage: 0, sorrow: 0, dread: 0 };
    w.calamityRaisedToday = false;
  }
}

/** Advance the world by one tick (one in-game hour). */
export function step(world: World, rng: Rng, policy: Policy = scriptedPolicy): void {
  stepWards(world, rng);
  stepMerchants(world, rng);
  const order = [...world.participants].sort(() => rng.next() - 0.5);
  for (const me of order) {
    if (!rng.chance(TUNING.actChance)) continue;
    const options = legalActions(world, me);
    const choice = policy(world, me, options, rng);
    act(world, me, choice, rng);
  }
  if (hourOf(world.tick) === TICKS_PER_DAY - 1) endOfDay(world);
  world.tick += 1;
}

/** Run a whole simulation from a seed. Same seed, same result. */
export function simulate(seed: number, ticks: number, onTick?: (w: World) => void): World {
  const world = createWorld(seed);
  const rng = createRng(seed);
  for (let i = 0; i < ticks; i++) {
    step(world, rng);
    onTick?.(world);
  }
  return world;
}

// ---------------------------------------------------------------------------
// Summaries (for the sim printout, the server API and the Observatory)
// ---------------------------------------------------------------------------

export interface WorldSummary {
  tick: number;
  day: number;
  hour: number;
  night: boolean;
  wards: { id: WardId; name: string; grudge: GrudgeMix; total: number; wild: number }[];
  merchants: { id: string; name: string; ward: WardId; ember: number; closed: boolean; prices: Record<string, number> }[];
  leaderboard: { id: string; name: string; kind: string; points: number; ember: number; talismans: number }[];
  /** templateId and params let clients re-run arbiterCheck against the rules in force. */
  rules: { id: number; text: string; author: string; cost: number; templateId: string; params: RuleParams }[];
  burnedEmber: number;
}

export function summarize(world: World): WorldSummary {
  const round = (x: number) => Math.round(x * 10) / 10;
  return {
    tick: world.tick,
    day: dayOf(world.tick),
    hour: hourOf(world.tick),
    night: isNight(world.tick),
    wards: WARDS.map((id) => {
      const w = world.wards[id];
      return {
        id, name: WARD_INFO[id].name,
        grudge: { rage: round(w.grudge.rage), sorrow: round(w.grudge.sorrow), dread: round(w.grudge.dread) },
        total: round(totalGrudge(w.grudge)), wild: w.wild.length,
      };
    }),
    merchants: world.merchants.map((m) => ({
      id: m.id, name: m.name, ward: m.ward, ember: m.ember, closed: m.closed,
      prices: Object.fromEntries(m.items.map((i) => [i.id, quoteSellPrice(i, { wardGrudge: totalGrudge(world.wards[m.ward].grudge), buyerIsOathbreaker: false }).price])),
    })),
    leaderboard: [...world.participants]
      .sort((a, b) => b.points - a.points)
      .map((p) => ({ id: p.id, name: p.name, kind: p.kind, points: p.points, ember: p.ember, talismans: p.talismans.length })),
    rules: world.rules.map((r) => ({ id: r.id, text: renderRule(r.templateId, r.params), author: r.author, cost: r.cost, templateId: r.templateId, params: { ...r.params } })),
    burnedEmber: world.burnedEmber,
  };
}

/** Server-owned Satchel and ink. One process, atomic JSON replacement, no keys or sessions in this file. */
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { fileChanged } from "./remoteFiles";
import { dirname } from "node:path";
import { randomUUID } from "node:crypto";
import {
  DEFAULT_PLAYER_CURSE, RIVAL_STAGE, SALVE_HEAL_FRACTION, TICKS_PER_DAY, WARD_INFO,
  curseById, growth, levelForXp, log, makeCombatant, nextLevelXp, equipmentSnapshot, xpForWin,
  type BattleState, type Outcome, type PlayerAction, type World,
} from "@grudgebound/shared";
import { AuthError } from "./auth";
import type { ChainReceipt, ChainWriter } from "./chain";
import { displayName } from "./rules";
import { explorationView, interact, newExploration, seeTip, validExploration, type Exploration } from "./exploration";
import { newProfile, validProfile, profileChoice, profileView, type ArrivalProfile } from "./profile";
import { rivalEndLine, rivalRefusal, rivalView, tideOpen } from "./rival";

export const CARRY_LIMIT = 5;
/** Existing built-in partner, not a minted or tradable asset. Scoped to the signed-in account's store. */
export const STARTER_TOKEN_ID = "starter";
export type Supply = "blank" | "gilded" | "salve";
/** Kessa's welcome (owner's choice, 7 Nov): enough for a first bind without Ember. Granted once, server-side only. */
export const STARTER_KIT: Readonly<Record<Supply, number>> = { blank: 3, gilded: 0, salve: 1 };
export interface Ink { hp: number; maxHp: number }
export interface Partner extends Ink { tokenId: string; speciesId: string; level: number }
export interface LevelView { level: number; xp: number; next: number | null }
/** What a finished battle taught: shown on the battle screen and stored before the answer is sent. */
export interface BattleProgress { gained: number; level: number; before: number; next: number | null; xp: number; rivalLine?: string }
export interface SatchelView {
  carried: string[]; case: string[]; supplies: Record<Supply, number>; keepsakes: string[];
  ink: Record<string, Ink>; ember: number | null; points: number | null;
  exploration: ReturnType<typeof explorationView>;
  profile: ReturnType<typeof profileView>;
  levels: Record<string, LevelView>;
  rival: ReturnType<typeof rivalView>;
  /** The ferry takes this player to Tide. */
  tideOpen: boolean;
}
interface Pocket {
  initialized: boolean; carried: string[]; supplies: Record<Supply, number>; keepsakes: string[];
  ink: Record<string, Ink>; personalTick: number; pendingBindings: Record<string, Supply>;
  exploration: Exploration; pendingTideBindings: string[];
  profile: ArrivalProfile;
  /** Experience per talisman id (the virtual starter is "starter"). */
  xp: Record<string, number>;
}
const empty = (): Pocket => ({ initialized: false, carried: [], supplies: { blank: 0, gilded: 0, salve: 0 },
  keepsakes: [], ink: {}, personalTick: 0, pendingBindings: {}, exploration: newExploration(), pendingTideBindings: [], profile: newProfile(), xp: {} });

/** A Tide binding that settles clears the Net mender's condition, and records that it was cleared by binding. */
function tideBound(e: Exploration) { if (!e.flags.includes("tide-cleared")) e.flags.push("tide-cleared", "tide-bound"); }
export function supplyForItem(itemId: string): Supply | null {
  // Keep the existing merchant item id as a purchase alias for plain blanks.
  return ({ "blank-talisman": "blank", "binding-seal": "blank", "gilded-talisman": "gilded", "ink-salve": "salve" } as Record<string, Supply>)[itemId] ?? null;
}

export function menderQuote(world: World) {
  const sorrow = world.wards.lantern.grudge.sorrow;
  return { price: Math.round(8 * (1 + sorrow / 100)),
    reason: `Sorrow is ${sorrow >= 50 ? "high" : "low"} in ${WARD_INFO.lantern.name}, so ink costs ${Math.round(8 * (1 + sorrow / 100))} Ember.` };
}

export class Satchels {
  private pockets = new Map<string, Pocket>();
  private readonly healing = new Set<string>();
  private readonly binding = new Set<string>();
  constructor(private readonly world: World, private readonly chain: ChainWriter | null,
    private readonly options: { file?: string; gameAccount?: string; canSignFor?: (account: string) => boolean; starterKit?: Readonly<Record<Supply, number>>; mintStarters?: boolean } = {}) {
    if (options.file && existsSync(options.file)) {
      const saved = JSON.parse(readFileSync(options.file, "utf8")) as { version: number; players: [string, Pocket][] };
      if (![1, 2, 3, 4].includes(saved.version) || !Array.isArray(saved.players)) throw new Error("Unsupported Satchel store");
      for (const [account, p] of saved.players) {
        if (saved.version === 1) { p.exploration = newExploration(); p.pendingTideBindings = []; }
        if (saved.version < 3) p.profile = newProfile(p.initialized);
        if (saved.version < 4) p.xp = {};
        if (typeof account !== "string" || !Array.isArray(p.carried) || p.carried.length > CARRY_LIMIT ||
            ![p.supplies.blank, p.supplies.gilded, p.supplies.salve, p.personalTick].every((n) => Number.isSafeInteger(n) && n >= 0) ||
            !p.ink || !p.pendingBindings || !Array.isArray(p.keepsakes) || !validExploration(p.exploration)
            || !validProfile(p.profile) || !p.xp || typeof p.xp !== "object" || !Object.values(p.xp).every((n) => Number.isSafeInteger(n) && n >= 0) || !Array.isArray(p.pendingTideBindings) || !p.pendingTideBindings.every(id => typeof id === "string")) throw new Error("Invalid Satchel store");
        this.pockets.set(account, p);
      }
    }
  }

  private read(account: string): Pocket { return structuredClone(this.pockets.get(account) ?? empty()); }
  private update(account: string, change: (p: Pocket) => void) {
    const p = this.read(account); change(p);
    const next = new Map(this.pockets); next.set(account, p);
    if (this.options.file) {
      mkdirSync(dirname(this.options.file), { recursive: true });
      const tmp = `${this.options.file}.tmp`;
      writeFileSync(tmp, JSON.stringify({ version: 4, players: [...next] }), { mode: 0o600 });
      renameSync(tmp, this.options.file);
      fileChanged(this.options.file);
    }
    this.pockets = next;
  }

  supplies(account: string) { return this.read(account).supplies; }
  profile(account: string) { return profileView(this.read(account).profile); }
  async completeProfile(account: string, body: Record<string, unknown>) {
    this.update(account, p => {
      const choice = profileChoice(body, p.profile);
      if (p.profile.complete) {
        if (["name", "look", "pronouns", "starter"].some(k => choice[k as keyof typeof choice] !== p.profile[k as keyof ArrivalProfile]))
          throw new AuthError(409, "Your arrival is already recorded. Your first partner cannot be chosen again.");
        // An owner-approved contract upgrade can settle the previously chosen virtual partner.
        // Preserve its ink and species; this is not another welcome grant.
        if (p.profile.mint === "virtual" && !p.profile.legacyStarter && this.chain && this.options.mintStarters)
          p.profile.mint = "pending";
        return;
      }
      p.profile = { ...p.profile, ...choice, complete: true,
        mint: this.chain && this.options.mintStarters && !p.profile.legacyStarter ? "pending" : "virtual" };
      // Choosing a species cannot heal a partner already used by an older client.
      if (!p.profile.legacyStarter) delete p.ink[STARTER_TOKEN_ID];
    });
    const chosen = this.read(account).profile;
    if (chosen.mint === "pending" && this.chain && this.options.mintStarters) {
      const minted = await this.chain.mintStarter(account, chosen.starter);
      this.update(account, p => {
        p.profile.tokenId = minted.tokenId; p.profile.mint = "done"; p.profile.proofUrl = minted.url;
        p.carried = p.carried.map(id => id === STARTER_TOKEN_ID ? minted.tokenId : id);
        if (p.ink[STARTER_TOKEN_ID]) p.ink[minted.tokenId] = p.ink[STARTER_TOKEN_ID]!;
        delete p.ink[STARTER_TOKEN_ID];
        if (p.xp[STARTER_TOKEN_ID]) p.xp[minted.tokenId] = p.xp[STARTER_TOKEN_ID]!;
        delete p.xp[STARTER_TOKEN_ID];
      });
    }
    return { ...await this.view(account), message: `Welcome to Hollowmere, ${chosen.name}. Your partner is ready.` };
  }
  equipment(account: string) { return equipmentSnapshot(this.read(account).exploration.equipped); }
  level(account: string, tokenId: string): number { return levelForXp(this.read(account).xp[tokenId] ?? 0); }
  private arrived(p: Pocket) { return p.profile.complete || p.profile.legacyStarter; }
  /** Why this rival stage cannot start now, or null. */
  rivalRefusal(account: string, stage: unknown): string | null {
    const p = this.read(account); return rivalRefusal(p.exploration, stage, this.arrived(p));
  }
  tideOpen(account: string): boolean { return tideOpen(this.read(account).exploration); }
  /** What the rival says before the fight that can start now. */
  rivalLine(account: string): string { const p = this.read(account); return rivalView(p.exploration, p.profile.starter, p.profile.name, this.arrived(p)).line; }
  /**
   * A finished battle, applied once by the battle session that ended it: experience for a won or bound fight (none for
   * fleeing or losing), the rival's result, and the ink a level-up adds. Returns what to show on the battle screen.
   */
  finishBattle(account: string, tokenId: string, speciesId: string, outcome: Outcome, foeLevel: number, rival: number | undefined, hpLeftFraction: number): BattleProgress {
    let progress: BattleProgress | undefined;
    this.update(account, (p) => {
      const xp = p.xp[tokenId] ?? 0; const before = levelForXp(xp);
      const gained = outcome === "won" || (outcome === "bound" && !rival) ? xpForWin(foeLevel, rival) : 0;
      const total = xp + gained; const level = levelForXp(total);
      if (gained) p.xp[tokenId] = total;
      if (level > before && p.ink[tokenId]) {
        const emotion = curseById(speciesId).emotion;
        p.ink[tokenId]!.hp += growth(emotion, level).hp - growth(emotion, before).hp;
      }
      let rivalLine: string | undefined;
      if (rival && (outcome === "won" || outcome === "lost")) {
        const flag = outcome === "won" ? `rival-${rival}-won` : rival === 1 ? "rival-1-lost" : "";
        if (flag && !p.exploration.flags.includes(flag)) p.exploration.flags.push(flag);
        rivalLine = rivalEndLine(rival, outcome, hpLeftFraction, p.profile.name);
      }
      progress = { gained, level, before, next: nextLevelXp(level), xp: total, ...(rivalLine ? { rivalLine } : {}) };
    });
    return progress!;
  }
  rivalPoints(stage: number) { return RIVAL_STAGE[stage]?.points ?? 0; }
  async explore(account: string, id: unknown) {
    let message = "";
    this.update(account, p => { message = interact(p, id); });
    return { ...await this.view(account), message };
  }
  seeTip(account: string, id: unknown) { this.update(account, p => seeTip(p.exploration, id)); }
  async equip(account: string, itemId: unknown) {
    if (typeof itemId !== "string") throw new AuthError(400, "Choose an equipment item.");
    this.update(account, p => {
      if (itemId !== "" && (!equipmentSnapshot(itemId) || !p.exploration.gear.includes(itemId))) throw new AuthError(403, "You do not own this equipment.");
      p.exploration.equipped = itemId;
    });
    return { ...await this.view(account), message: itemId ? "Charm equipped. Its bonus applies to your next battle." : "Charm unequipped." };
  }
  addSupply(account: string, supply: Supply, count = 1) {
    if (!Number.isSafeInteger(count) || count <= 0) throw new Error("Invalid supply count");
    this.update(account, (p) => {
      if (!Number.isSafeInteger(p.supplies[supply] + count)) throw new Error("Supply count overflow");
      p.supplies[supply] += count;
    });
  }
  resourceError(account: string, action: PlayerAction): string | null {
    const supplies = this.supplies(account);
    if (action.kind === "seal" && supplies[action.gilded ? "gilded" : "blank"] <= 0)
      return action.gilded ? "No gilded talismans: buy one from the Broker." : "No blank talismans: buy one from the Broker.";
    if (action.kind === "item" && supplies.salve <= 0) return "No Ink Salves: buy one from the Broker.";
    return null;
  }
  /** Apply once per accepted turn, including failed seal rolls. Invalid/rejected moves spend nothing. */
  applyTurn(account: string, tokenId: string, battleId: string, action: PlayerAction, state: BattleState, ward?: string) {
    this.update(account, (p) => {
      const supply = action.kind === "seal" ? (action.gilded ? "gilded" : "blank") : action.kind === "item" ? "salve" : null;
      if (supply) {
        if (p.supplies[supply] <= 0) throw new Error("Supply already spent");
        p.supplies[supply]--;
        if (state.outcome === "bound") p.pendingBindings[battleId] = supply;
      }
      p.ink[tokenId] = { hp: state.player.hp, maxHp: state.player.maxHp };
      if (ward === "tide" && p.exploration.flags.includes("net-accepted")) {
        if (state.outcome === "won" && !p.exploration.flags.includes("tide-cleared")) p.exploration.flags.push("tide-cleared");
        if (state.outcome === "bound" && !p.pendingTideBindings.includes(battleId)) p.pendingTideBindings.push(battleId);
      }
    });
    if (state.outcome === "bound") this.binding.add(account);
  }
  /** A failed points submission after a successful mint must NOT refund another binding supply. */
  settleBinding(account: string, battleId: string, receipt: ChainReceipt) {
    if (receipt.status === "pending" || !this.read(account).pendingBindings[battleId]) return;
    this.update(account, (p) => {
      const supply = p.pendingBindings[battleId]!;
      if (receipt.status === "failed" && !receipt.tokenId) p.supplies[supply]++;
      if (p.pendingTideBindings.includes(battleId) && (receipt.status === "done" || receipt.tokenId)) tideBound(p.exploration);
      p.pendingTideBindings = p.pendingTideBindings.filter(id => id !== battleId);
      delete p.pendingBindings[battleId];
    });
    this.binding.delete(account);
  }
  completeOfflineBinding(account: string, battleId: string) {
    if (this.read(account).pendingBindings[battleId]) this.update(account, (p) => {
      delete p.pendingBindings[battleId];
      if (p.pendingTideBindings.includes(battleId)) tideBound(p.exploration);
      p.pendingTideBindings = p.pendingTideBindings.filter(id => id !== battleId);
    });
    this.binding.delete(account);
  }

  private async owned(account: string): Promise<{ tokenId: string; speciesId: string }[]> {
    const tokens = this.chain ? (await this.chain.talismansOf(account)).map((t) => ({ tokenId: t.token_id, speciesId: t.meta.species })) : [];
    const p = this.read(account).profile;
    return p.mint === "done" ? tokens : [{ tokenId: STARTER_TOKEN_ID, speciesId: p.starter }, ...tokens];
  }
  private sync(account: string, owned: { tokenId: string }[]) {
    const p = this.read(account); const ids = new Set(owned.map((t) => t.tokenId));
    const carried = p.initialized ? p.carried.filter((id) => ids.has(id)) : owned.slice(0, CARRY_LIMIT).map((t) => t.tokenId);
    if (!p.initialized || JSON.stringify(carried) !== JSON.stringify(p.carried))
      this.update(account, (p) => {
        // First Satchel for this account: add the starter kit once (it adds to anything bought before the first view).
        if (!p.initialized && this.options.starterKit) for (const k of ["blank", "gilded", "salve"] as const) p.supplies[k] += this.options.starterKit[k];
        p.carried = carried; p.initialized = true;
      });
  }
  private inkFor(account: string, token: { tokenId: string; speciesId: string }): Ink {
    const species = curseById(token.speciesId);
    const maxHp = makeCombatant(species).maxHp + growth(species.emotion, this.level(account, token.tokenId)).hp;
    return { hp: Math.max(0, Math.min(maxHp, this.read(account).ink[token.tokenId]?.hp ?? maxHp)), maxHp };
  }
  async view(account: string): Promise<SatchelView> {
    const [owned, ember, points] = await Promise.all([this.owned(account), this.chain?.ember(account) ?? null, this.chain?.points(account) ?? null]);
    this.sync(account, owned); const p = this.read(account);
    return { carried: p.carried, case: owned.filter((t) => !p.carried.includes(t.tokenId)).map((t) => t.tokenId),
      supplies: p.supplies, keepsakes: p.keepsakes, exploration: explorationView(p.exploration, p.keepsakes, p.profile),
      profile: profileView(p.profile),
      levels: Object.fromEntries(owned.map((t) => { const xp = p.xp[t.tokenId] ?? 0; const level = levelForXp(xp); return [t.tokenId, { level, xp, next: nextLevelXp(level) }]; })),
      rival: rivalView(p.exploration, p.profile.starter, p.profile.name, this.arrived(p)), tideOpen: tideOpen(p.exploration),
      ink: Object.fromEntries(owned.map((t) => [t.tokenId, this.inkFor(account, t)])), ember, points };
  }
  async carry(account: string, tokenId: unknown, carry: unknown): Promise<SatchelView> {
    if (typeof tokenId !== "string" || typeof carry !== "boolean") throw new AuthError(400, "Expected tokenId and carry:boolean.");
    const owned = await this.owned(account); this.sync(account, owned);
    if (!owned.some((t) => t.tokenId === tokenId)) throw new AuthError(403, "Choose a talisman you own.");
    this.update(account, (p) => {
      if (carry && !p.carried.includes(tokenId)) {
        if (p.carried.length >= CARRY_LIMIT) throw new AuthError(409, "The Satchel carries at most 5 talismans. Leave one in the Case first.");
        p.carried.push(tokenId);
      } else if (!carry) p.carried = p.carried.filter((id) => id !== tokenId);
    });
    return this.view(account);
  }
  async partner(account: string, tokenId?: unknown, speciesId?: unknown): Promise<Partner> {
    const profile = this.read(account).profile;
    if (profile.mint === "pending") throw new AuthError(409, "Your partner is still being recorded. Return to Kessa and retry your saved choice.");
    if (tokenId !== undefined && typeof tokenId !== "string") throw new AuthError(400, "tokenId must be a talisman id.");
    const owned = await this.owned(account); this.sync(account, owned);
    const p = this.read(account);
    const token = owned.find((t) => tokenId !== undefined ? t.tokenId === tokenId
      : p.carried.includes(t.tokenId) && t.speciesId === (speciesId ?? profile.starter));
    if (!token || !p.carried.includes(token.tokenId)) throw new AuthError(403, "Choose a partner carried in your Satchel.");
    const ink = this.inkFor(account, token);
    if (!ink.hp) throw new AuthError(409, "Visit the Mender or rest");
    if (!profile.complete && !profile.legacyStarter) this.update(account, p => { p.profile.legacyStarter = true; });
    return { ...token, ...ink, level: this.level(account, token.tokenId) };
  }

  isRestoring(account: string) { return this.healing.has(account); }
  isBinding(account: string) { return this.binding.has(account); }
  private assertNotRestoring(account: string) {
    if (this.isRestoring(account)) throw new AuthError(409, "Wait for the Mender's receipt before continuing.");
  }
  async restore(account: string, tokenId: unknown) {
    this.assertNotRestoring(account);
    if (!this.chain || !this.options.gameAccount || !this.options.canSignFor?.(account)) throw new AuthError(503, "The Mender needs Ember settlement.");
    this.healing.add(account);
    try {
      const owned = await this.owned(account); const token = owned.find((t) => t.tokenId === tokenId);
      if (!token) throw new AuthError(403, "Choose a talisman you own.");
      const ink = this.inkFor(account, token);
      if (ink.hp === ink.maxHp) throw new AuthError(409, "This talisman's ink is already full.");
      const quote = menderQuote(this.world);
      if (await this.chain.ember(account) < quote.price) throw new AuthError(402, `The Mender needs ${quote.price} Ember.`);
      const jobId = randomUUID(); const chain = await this.chain.job("shop", jobId, account, async ({ relay, tx }) => {
        const paid = await relay(account, this.chain!.cfg.itemsContract, "mt_transfer", {
          receiver_id: this.options.gameAccount!, token_id: "ember", amount: String(quote.price), memo: `restore ink ${token.tokenId}`,
        }, 1n); tx("pay", paid.hash);
      }, () => {
        try {
          this.update(account, (p) => { p.ink[token.tokenId] = { hp: ink.maxHp, maxHp: ink.maxHp }; });
          log(this.world, { type: "mender", ward: "lantern", actor: account, text: `${displayName(account)} restored a talisman's ink for ${quote.price} Ember. ${quote.reason}` });
        } finally { this.healing.delete(account); }
      }, () => { this.healing.delete(account); });
      return { jobId, ...quote, tokenId: token.tokenId, chain };
    } catch (e) { this.healing.delete(account); throw e; }
  }
  async rest(account: string) {
    this.assertNotRestoring(account);
    const owned = await this.owned(account);
    this.update(account, (p) => {
      p.ink = {};
      const clock = Math.max(p.personalTick, this.world.tick);
      // Strictly the next 06:00, including when it is already morning. Never step the shared world.
      p.personalTick = (Math.floor((clock - 6) / TICKS_PER_DAY) + 1) * TICKS_PER_DAY + 6;
    });
    const personalTick = this.read(account).personalTick;
    return { personalTick, hour: 6, ink: Object.fromEntries(owned.map((t) => [t.tokenId, this.inkFor(account, t)])) };
  }
  async useSalve(account: string, tokenId: unknown) {
    this.assertNotRestoring(account);
    const owned = await this.owned(account); const token = owned.find((t) => t.tokenId === tokenId);
    if (!token) throw new AuthError(403, "Choose a talisman you own.");
    const ink = this.inkFor(account, token);
    if (ink.hp === ink.maxHp) throw new AuthError(409, "This talisman's ink is already full.");
    if (!this.supplies(account).salve) throw new AuthError(409, "No Ink Salves: buy one from the Broker.");
    ink.hp = Math.min(ink.maxHp, ink.hp + Math.round(ink.maxHp * SALVE_HEAL_FRACTION));
    this.update(account, (p) => { p.supplies.salve--; p.ink[token.tokenId] = ink; });
    return { tokenId: token.tokenId, ink, supplies: this.supplies(account) };
  }
}

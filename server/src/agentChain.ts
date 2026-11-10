/**
 * AI agents act through their own NEAR accounts (P3-C2, P3-A1). Owner decision 8 Nov: **mirror the sim**: the
 * simulation stays the source of agent behaviour; the chain carries the proof.
 *
 * - Accounts: `agent-<id>.<gameAccount>` on testnet, created in ONE transaction with two function-call keys and no
 *   full-access key: the items key may only call `pay_minter` (Ember to the game, no deposit), the rite key only
 *   `propose_rule`. A leaked key can at most pay the house or propose a rule Kessa still judges.
 * - Points: after each tick, ONE `submit_battle` from the game account carries every agent's points delta.
 * - Proposals: an agent's rule goes through the same path as a player's (Kessa's verdict, the agent's own relayed
 *   `propose_rule`, then the Arbiter's `resolve_rule`); it joins the world only when resolved.
 * - Buys: paid on chain with `pay_minter`; if the agent's chain Ember is short, the game mints the shortfall, never
 *   more than its simulated Ember and never more than the daily cap.
 * - Guardrail: only the four methods below, at most `actionsPerDay` agent-signed actions per in-game day, and
 *   `AGENT_CHAIN=off` keeps the sim unchanged. Keys are encrypted at rest with the auth store's key, never served.
 */
import { createCipheriv, createDecipheriv, createHash, randomBytes, randomUUID } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { fileChanged } from "./remoteFiles";
import { dirname } from "node:path";
import { KeyPair, type KeyPairString } from "near-api-js";
import {
  arbiterCheck, dayOf, log, type Participant, type ParticipantAction, type RuleParams, type StepHooks, type World,
} from "@grudgebound/shared";
import type { ChainReceipt, ChainWriter } from "./chain";
import { registerAgentAccount, type ChainRules } from "./rules";

/** Every chain method an agent action may use. Anything else is refused before it is sent. */
export const AGENT_METHODS = new Set(["submit_battle", "propose_rule", "pay_minter", "mint_ember"]);

export interface StoredAgent { id: string; account: string; itemsKey: string; riteKey: string; ready: boolean }
export interface AgentData { version: 1; agents: StoredAgent[] }
export interface AgentStore { data: AgentData; save(): void }

/** AES-256-GCM like the auth store (same file format and key), so agent keys are never on disk in clear. */
export class EncryptedAgentStore implements AgentStore {
  data: AgentData = { version: 1, agents: [] };
  constructor(private readonly file: string, private readonly key: Buffer) {
    if (key.length !== 32) throw new Error("The agent store key must be 32 bytes");
    if (existsSync(file)) {
      const packed = Buffer.from(readFileSync(file, "utf8"), "base64");
      const decipher = createDecipheriv("aes-256-gcm", key, packed.subarray(0, 12));
      decipher.setAuthTag(packed.subarray(12, 28));
      this.data = JSON.parse(Buffer.concat([decipher.update(packed.subarray(28)), decipher.final()]).toString("utf8"));
      if (this.data.version !== 1 || !Array.isArray(this.data.agents)) throw new Error("Unsupported agent store");
    }
  }
  save() {
    mkdirSync(dirname(this.file), { recursive: true });
    const iv = randomBytes(12);
    const cipher = createCipheriv("aes-256-gcm", this.key, iv);
    const body = Buffer.concat([cipher.update(JSON.stringify(this.data), "utf8"), cipher.final()]);
    const tmp = `${this.file}.tmp`;
    writeFileSync(tmp, Buffer.concat([iv, cipher.getAuthTag(), body]).toString("base64"), { mode: 0o600 });
    renameSync(tmp, this.file);
    fileChanged(this.file);
  }
}

export interface AgentLimits {
  /** Most Ember the game mints to one agent per in-game day to mirror its simulated purse. */
  dailyEmberCap: number;
  /** Most agent-signed chain actions (proposals, payments) per agent per in-game day. */
  actionsPerDay: number;
}
export const DEFAULT_AGENT_LIMITS: AgentLimits = { dailyEmberCap: 200, actionsPerDay: 6 };

export interface AgentActionView { kind: string; text: string; tick: number; status: string; url?: string }
export interface AgentView {
  id: string; name: string; account: string; ready: boolean;
  points: number | null; ember: number | null; actions: AgentActionView[];
}

interface Noted { kind: string; text: string; tick: number; receipt?: () => ChainReceipt | undefined }

export class AgentChain {
  /** Simulated points already mirrored on chain, per agent. */
  private readonly mirrored = new Map<string, number>();
  private readonly used = new Map<string, { day: number; actions: number; minted: number }>();
  private readonly history = new Map<string, Noted[]>();
  /** This tick's agent choices, hashed into the batched submit_battle so the mirror can be checked. */
  private tickLog: unknown[] = [];
  private submitting = false;

  constructor(
    private readonly world: World,
    private readonly chain: ChainWriter,
    private readonly rules: ChainRules | null,
    private readonly store: AgentStore,
    private readonly opts: { gameAccount: string; network: string; limits?: AgentLimits },
  ) {
    if (opts.network !== "testnet") throw new Error("Agent accounts are testnet only");
    for (const a of this.agents()) {
      registerAgentAccount(this.accountOf(a.id), a.id, a.name);
      this.mirrored.set(a.id, a.points);
    }
  }

  private get limits(): AgentLimits { return this.opts.limits ?? DEFAULT_AGENT_LIMITS; }
  agents(): Participant[] { return this.world.participants.filter((p) => p.kind === "agent"); }
  accountOf(id: string): string { return `agent-${id}.${this.opts.gameAccount}`; }
  private stored(id: string): StoredAgent | undefined { return this.store.data.agents.find((a) => a.id === id); }
  private ready(id: string): boolean { return !!this.stored(id)?.ready; }

  /** The agent's limited key for `contractId` (the rite key when no contract is named). Never served. */
  keyFor(account: string, contractId?: string): KeyPairString | undefined {
    const a = this.store.data.agents.find((x) => x.account === account && x.ready);
    if (!a) return undefined;
    return (contractId === this.chain.cfg.itemsContract ? a.itemsKey : a.riteKey) as KeyPairString;
  }

  /** Creates every missing agent account (idempotent; keys saved before any transaction leaves). */
  async provision(): Promise<void> {
    for (const me of this.agents()) {
      let a = this.stored(me.id);
      if (!a) {
        a = { id: me.id, account: this.accountOf(me.id), itemsKey: KeyPair.fromRandom("ed25519").toString(),
          riteKey: KeyPair.fromRandom("ed25519").toString(), ready: false };
        this.store.data.agents.push(a);
        this.store.save();
      }
      if (a.ready) continue;
      try {
        await this.chain.provisionLimitedAccount(a.account, [
          { publicKey: KeyPair.fromString(a.itemsKey as KeyPairString).getPublicKey().toString(), contractId: this.chain.cfg.itemsContract, methods: ["pay_minter"] },
          { publicKey: KeyPair.fromString(a.riteKey as KeyPairString).getPublicKey().toString(), contractId: this.chain.cfg.riteContract, methods: ["propose_rule"] },
        ]);
        a.ready = true;
        this.store.save();
      } catch (e) {
        console.warn(`Agents: ${a.account} could not be created yet (${(e as Error).message.slice(0, 160)}); it stays sim-only.`);
      }
    }
  }

  /** Counts one agent-signed action against today's limit; false when the limit is reached. */
  private take(id: string): boolean {
    const day = dayOf(this.world.tick);
    const u = this.used.get(id);
    const today = u && u.day === day ? u : { day, actions: 0, minted: 0 };
    if (today.actions >= this.limits.actionsPerDay) return false;
    today.actions++;
    this.used.set(id, today);
    return true;
  }
  private recordMint(id: string, amount: number) {
    const day = dayOf(this.world.tick);
    const u = this.used.get(id);
    const today = u && u.day === day ? u : { day, actions: 0, minted: 0 };
    today.minted += amount;
    this.used.set(id, today);
  }
  private mintRoom(id: string): number {
    const u = this.used.get(id);
    return this.limits.dailyEmberCap - (u && u.day === dayOf(this.world.tick) ? u.minted : 0);
  }
  private guard(method: string) {
    if (!AGENT_METHODS.has(method)) throw new Error(`agent guardrail: ${method} is not an agent action`);
  }
  private note(id: string, n: Noted) {
    const list = this.history.get(id) ?? [];
    list.push(n);
    if (list.length > 10) list.shift();
    this.history.set(id, list);
  }
  private simOnly(me: Participant, what: string) {
    this.note(me.id, { kind: "sim-only", text: what, tick: this.world.tick });
  }

  /** Hooks for step(): proposals go to the Rite, buys are paid on chain. */
  hooks(): StepHooks {
    return {
      onAgentProposal: (world, me, templateId, params) => {
        if (!this.rules || !this.ready(me.id)) return false;
        if (!this.take(me.id)) { this.simOnly(me, `proposal kept in the town's ledger only (daily limit)`); return false; }
        // Mirror the sim's "one public challenge a day" so a rejected agent doesn't retry every tick.
        if (!arbiterCheck(templateId, params, world.rules).accepted) me.rejectedDay = dayOf(world.tick);
        this.tickLog.push({ agent: me.id, type: "proposeRule", templateId, params });
        void this.propose(me, templateId, params);
        return true;
      },
      afterAction: (_world, me, action, emberBefore) => {
        if (me.kind !== "agent") return;
        if (action.type !== "proposeRule") this.tickLog.push({ agent: me.id, ...action });
        if (action.type === "buy" && me.ember < emberBefore) void this.pay(me, action, emberBefore - me.ember, emberBefore);
      },
    };
  }

  private async propose(me: Participant, templateId: string, params: RuleParams): Promise<void> {
    const r = await this.rules!.propose({ account: this.accountOf(me.id), templateId, params });
    if (r.status !== 200) {
      const why = "error" in r.body ? r.body.error : "";
      log(this.world, { type: "agent", actor: me.id,
        text: r.status === 402 ? `Kessa turns ${me.name} away: not enough standing.` : `${me.name}'s proposal did not reach the Rite (${why}).` });
      this.note(me.id, { kind: "propose", text: `turned away: ${why}`, tick: this.world.tick });
      return;
    }
    const a = r.body as { jobId: string; accepted: boolean; text: string; cost: number };
    if (a.accepted) {
      // The chain took the cost from the agent's own account; take it in the sim too, without mirroring it twice.
      const paid = Math.min(me.points, a.cost);
      me.points -= paid;
      this.mirrored.set(me.id, (this.mirrored.get(me.id) ?? 0) - paid);
    }
    this.note(me.id, { kind: "propose", text: `${a.accepted ? "proposed" : "was refused"}: "${a.text}"`, tick: this.world.tick,
      receipt: () => this.chain.ruleReceipt(a.jobId) });
  }

  private async pay(me: Participant, action: Extract<ParticipantAction, { type: "buy" }>, price: number, simEmber: number): Promise<void> {
    if (!this.ready(me.id)) return;
    if (!this.take(me.id)) { this.simOnly(me, `bought ${action.itemId} in the town's ledger only (daily limit)`); return; }
    const account = this.accountOf(me.id);
    const items = this.chain.cfg.itemsContract;
    const id = randomUUID();
    const text = `bought ${action.itemId} from ${action.merchantId} for ${price} Ember`;
    await this.chain.job("agent", id, account, async ({ call, relay, tx }) => {
      const have = await this.chain.ember(account);
      if (have < price) {
        const shortfall = price - have;
        // Never more than the agent's simulated purse, never more than the daily spend cap.
        if (shortfall > simEmber - have || shortfall > this.mintRoom(me.id)) throw new Error("spend cap reached: the purchase stays in the town's ledger");
        this.guard("mint_ember");
        const f = await call(items, "mint_ember", { to: account, amount: String(shortfall) });
        this.recordMint(me.id, shortfall);
        tx("fund", f.hash);
      }
      this.guard("pay_minter");
      const p = await relay(account, items, "pay_minter", { amount: String(price), memo: `agent buy ${action.itemId}` });
      tx("pay", p.hash);
    }, (receipt) => {
      const url = receipt.txs.at(-1)?.url;
      log(this.world, { type: "agent", actor: me.id, text: `${me.name} ${text}, paid from its own account.`, ...(url ? { url } : {}) });
    });
    this.note(me.id, { kind: "pay", text, tick: this.world.tick, receipt: () => this.chain.agentReceipt(id) });
  }

  /**
   * After each tick: one `submit_battle` from the game account with every agent's points delta. At most one in
   * flight; a failed write is retried with the next tick's deltas (no history back-fill after a restart).
   */
  afterStep(): void {
    const log_hash = createHash("sha256").update(JSON.stringify(this.tickLog)).digest("hex");
    this.tickLog = [];
    if (this.submitting) return;
    const changes: { account_id: string; delta: number }[] = [];
    const deltas = new Map<string, number>();
    for (const me of this.agents()) {
      if (!this.ready(me.id)) { this.mirrored.set(me.id, me.points); continue; }
      const delta = me.points - (this.mirrored.get(me.id) ?? me.points);
      if (delta !== 0) { changes.push({ account_id: this.accountOf(me.id), delta }); deltas.set(me.id, delta); }
      this.mirrored.set(me.id, me.points);
    }
    if (!changes.length) return;
    const battle_id = `agents-t${this.world.tick}`;
    const rite = this.chain.cfg.riteContract;
    this.submitting = true;
    void this.chain.job("agent", battle_id, this.opts.gameAccount, async ({ call, tx }) => {
      this.guard("submit_battle");
      try {
        const r = await call(rite, "submit_battle", { result: { battle_id, seed: this.world.seed, log_hash, changes } });
        tx("points", r.hash);
      } catch (e) {
        if (!/already recorded/.test((e as Error).message)) throw e;
      }
    }, (receipt) => {
      this.submitting = false;
      for (const [id, delta] of deltas) this.note(id, { kind: "points", text: `${delta > 0 ? "+" : ""}${delta} points`, tick: this.world.tick, receipt: () => receipt });
    }, () => {
      this.submitting = false;
      // Not on chain: hand the deltas back so the next tick's batch carries them.
      for (const [id, delta] of deltas) this.mirrored.set(id, (this.mirrored.get(id) ?? 0) - delta);
    });
  }

  private viewCache?: { at: number; value: Promise<AgentView[]> };

  /**
   * GET /agents: each agent's account, chain points and Ember, and its last chain actions. The route is public and
   * each fresh answer costs 10 RPC views, so answers are shared for `maxAgeMs` (anyone hammering it costs nothing).
   */
  view(maxAgeMs = 5000, now = Date.now()): Promise<AgentView[]> {
    if (this.viewCache && now - this.viewCache.at < maxAgeMs) return this.viewCache.value;
    const value = this.freshView();
    this.viewCache = { at: now, value };
    value.catch(() => { if (this.viewCache?.value === value) this.viewCache = undefined; });
    return value;
  }

  private async freshView(): Promise<AgentView[]> {
    return Promise.all(this.agents().map(async (me) => {
      const account = this.accountOf(me.id);
      const ready = this.ready(me.id);
      const [points, ember] = ready
        ? await Promise.all([this.chain.points(account).catch(() => null), this.chain.ember(account).catch(() => null)])
        : [null, null];
      const actions = (this.history.get(me.id) ?? []).map((n) => {
        const r = n.receipt?.();
        const url = r?.txs.at(-1)?.url;
        return { kind: n.kind, text: n.text, tick: n.tick, status: r?.status ?? (n.kind === "sim-only" ? "sim-only" : "done"), ...(url ? { url } : {}) };
      }).reverse();
      return { id: me.id, name: me.name, account, ready, points, ember, actions };
    }));
  }
}

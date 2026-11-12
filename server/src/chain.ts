/**
 * Battle results and rules on NEAR testnet (P2-C3, P2-C4, P2-C5).
 *
 * When a battle ends with a binding or an exorcism, the game master account:
 *   1. bind only: mints a talisman to the player on `items` (mint_talisman)
 *   2. both:      adds the points on `rite` (submit_battle), with a hash of the battle log so anyone can replay it
 *
 * The key is read from NEAR_GAME_PRIVATE_KEY, or else from NEAR CLI's legacy keychain
 * (~/.near-credentials/<network>/<game account>.json). With neither, the server runs off-chain as before.
 *
 * Players have no NEAR accounts until Phase 3 (email sign-in). Until then a battle names its account with
 * `account`, or the talisman goes to NEAR_GUEST_ACCOUNT (default guest.<game account>).
 *
 * Rules: the player's propose_rule goes through the relayer (NEP-366 delegate action, gas paid by
 * NEAR_RELAYER_ACCOUNT), so the player's own points pay for it; the game master then resolves it as the Arbiter.
 */
import { eventsIn, type ChainEvent, type RawChainEvent } from "./chainEvents";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
/** What callFunctionRaw / signAndSendTransaction resolve to (near-api-js does not export the type by name). */
type RpcTransactionResponse = Awaited<ReturnType<Account["signAndSendTransaction"]>>;
import { Account, JsonRpcProvider, KeyPairSigner, PublicKey, actions, getTransactionLastResult, type KeyPairString } from "near-api-js";
import type { Combatant, Omen, PlayerAction } from "@grudgebound/shared";

export interface ChainConfig {
  network: string;
  rpcUrl: string;
  gameAccount: string;
  itemsContract: string;
  riteContract: string;
  guestAccount: string;
  privateKey: KeyPairString;
  /** Where the key came from, for the startup log (never the key itself). */
  keySource: string;
  /** Pays the gas of players' relayed calls. */
  relayerAccount: string;
  /** From NEAR_RELAYER_PRIVATE_KEY; else read from the keychain when first needed. */
  relayerKey?: KeyPairString;
  /** Optional encrypted-store signer for authenticated testnet players (and agents, per contract). Never exposed over HTTP. */
  playerKey?: (account: string, contractId?: string) => KeyPairString | undefined;
}

/** A rule as the rite contract returns it. */
export interface ChainRule {
  id: number;
  template_id: string;
  params: string;
  author: string;
  cost: number;
  status: string;
  reason: string | null;
  created_ms: number;
}

/** The Rite is the source of truth for locked stakes and vow deadlines. */
export interface ChainVow {
  id: number; owner: string; template_id: string; params: string; stake: number;
  status: "Active" | "Kept" | "Broken"; created_ms: number; ends_ms: number;
}

/** One contract call: what it returned and the transaction hash. Swapped for a fake in tests. */
/** What a sent transaction gives back: its hash, the contract's return value, and the NEP-297 events it logged. */
export interface CallResult { hash: string; result: unknown; events?: RawChainEvent[] }
export type ContractCall = (contractId: string, methodName: string, args: Record<string, unknown>, deposit?: bigint) => Promise<CallResult>;

export interface BattleRecord {
  battleId: string;
  /** NEAR account that receives the talisman and the points. */
  account: string;
  outcome: "bound" | "won";
  points: number;
  seed: number;
  actions: PlayerAction[];
  wild: { speciesId: string; emotion: Combatant["emotion"]; omen: Omen };
  playerCurse: string;
  initial?: { playerHp: number; items: { salve: number; gilded: number }; equipment?: import("@grudgebound/shared").EquipmentSnapshot };
}

export interface ChainTx {
  kind: "mint" | "points" | "propose" | "resolve" | "repeal" | "pay" | "hand-over" | "swear" | "settle" | "refund" | "fund";
  hash: string;
  url: string;
}

/** A talisman as the items contract reports it. */
export interface ChainTalisman {
  token_id: string;
  owner: string;
  meta: { species: string; emotion: string; omen: string; level: number; wins: number };
  /** Ember earned, as a decimal string (U128). */
  purse: string;
}

export interface ChainReceipt {
  status: "done" | "pending" | "failed";
  network: string;
  account: string;
  /** Set when a talisman was minted, e.g. "tal-3". */
  tokenId?: string;
  /** Set when a rule was proposed: its id on the rite contract. */
  ruleId?: number;
  vowId?: number;
  txs: ChainTx[];
  error?: string;
}

const ACCOUNT_ID = /^(?=.{2,64}$)(([a-z\d]+[-_])*[a-z\d]+\.)*([a-z\d]+[-_])*[a-z\d]+$/;

export function isAccountId(s: unknown): s is string {
  return typeof s === "string" && ACCOUNT_ID.test(s);
}

export function chainConfigFromEnv(env: NodeJS.ProcessEnv = process.env): ChainConfig | null {
  if (env.NEAR_CHAIN === "off") return null;
  const network = env.NEAR_NETWORK?.trim() || "testnet";
  const gameAccount = env.NEAR_GAME_ACCOUNT?.trim();
  const itemsContract = env.NEAR_ITEMS_CONTRACT?.trim();
  const riteContract = env.NEAR_RITE_CONTRACT?.trim();
  if (!gameAccount || !itemsContract || !riteContract) return null;

  let privateKey = env.NEAR_GAME_PRIVATE_KEY?.trim() as KeyPairString | undefined;
  let keySource = "NEAR_GAME_PRIVATE_KEY";
  if (!privateKey) {
    const file = join(homedir(), ".near-credentials", network, `${gameAccount}.json`);
    try {
      privateKey = (JSON.parse(readFileSync(file, "utf8")) as { private_key?: KeyPairString }).private_key;
      keySource = file;
    } catch {
      return null;
    }
  }
  if (!privateKey) return null;

  return {
    network,
    rpcUrl: env.NEAR_RPC_URL?.trim() || defaultRpc(network),
    gameAccount,
    itemsContract,
    riteContract,
    guestAccount: env.NEAR_GUEST_ACCOUNT?.trim() || `guest.${gameAccount}`,
    privateKey,
    keySource,
    relayerAccount: env.NEAR_RELAYER_ACCOUNT?.trim() || `relayer.${gameAccount}`,
    relayerKey: (env.NEAR_RELAYER_PRIVATE_KEY?.trim() || undefined) as KeyPairString | undefined,
  };
}

/**
 * FastNEAR's free RPC: about 2.5 s per transaction from India on 4 Nov, against 8-12 s on rpc.testnet.near.org.
 */
export function defaultRpc(network: string): string {
  return network === "mainnet" ? "https://free.rpc.fastnear.com" : `https://${network === "testnet" ? "test" : network}.rpc.fastnear.com`;
}

export function explorerTxUrl(network: string, hash: string): string {
  return network === "mainnet" ? `https://nearblocks.io/txns/${hash}` : `https://${network}.nearblocks.io/txns/${hash}`;
}

/** sha256 of everything needed to replay the battle, stored on chain with the result. */
export function battleLogHash(r: Pick<BattleRecord, "seed" | "actions" | "wild" | "playerCurse" | "initial">): string {
  const log = JSON.stringify({ seed: r.seed, wild: r.wild.speciesId, omen: r.wild.omen, playerCurse: r.playerCurse, actions: r.actions,
    ...(r.initial ? { initial: r.initial } : {}) });
  return createHash("sha256").update(log).digest("hex");
}

/**
 * A player's call, relayed (NEP-366): the player signs a delegate action, the relayer account sends it and
 * pays the gas, and the contract sees the player as the caller. `result` is what the contract returned.
 */
export type RelayCall = (player: string, contractId: string, methodName: string, args: Record<string, unknown>, deposit?: bigint) => Promise<CallResult>;
/** A read-only contract call. */
export type ViewCall = (contractId: string, methodName: string, args: Record<string, unknown>) => Promise<unknown>;

export interface ChainOps {
  call: ContractCall;
  relay: RelayCall;
  view: ViewCall;
  createAccount?: (account: string, publicKey: string) => Promise<void>;
  /**
   * Creates `account` in ONE transaction with only function-call keys (no full-access key): each key may call
   * only `methods` on `contractId`. Idempotent: returns if the first key is already on the account.
   */
  createLimitedAccount?: (account: string, keys: LimitedKey[]) => Promise<void>;
}

/** A function-call access key: `publicKey` may call only `methods` on `contractId`. */
export interface LimitedKey { publicKey: string; contractId: string; methods: string[] }

export interface ChainJobOps {
  call: ContractCall; relay: RelayCall;
  tx: (kind: ChainTx["kind"], hash: string) => void;
  receipt: ChainReceipt;
}

const GAS = 30_000_000_000_000n;

function readKey(network: string, account: string): KeyPairString | undefined {
  try {
    const file = join(homedir(), ".near-credentials", network, `${account}.json`);
    return (JSON.parse(readFileSync(file, "utf8")) as { private_key?: KeyPairString }).private_key;
  } catch {
    return undefined;
  }
}

/** Throws with the first failing receipt's error (a relayed call can fail inside while the outer transaction succeeds). */
function checkReceipts(res: RpcTransactionResponse) {
  for (const r of res.receipts_outcome) {
    const status = r.outcome.status as { Failure?: unknown };
    if (status && typeof status === "object" && "Failure" in status) {
      throw new Error(`${r.outcome.executor_id} failed: ${JSON.stringify(status.Failure).slice(0, 240)}`);
    }
  }
}

/** The value returned by `contractId` inside a (possibly relayed) transaction. */
function returnedBy(res: RpcTransactionResponse, contractId: string): unknown {
  for (const r of res.receipts_outcome) {
    const status = r.outcome.status as { SuccessValue?: string };
    if (r.outcome.executor_id === contractId && status && typeof status === "object" && typeof status.SuccessValue === "string") {
      const text = Buffer.from(status.SuccessValue, "base64").toString("utf8");
      if (!text) return null;
      try { return JSON.parse(text); } catch { return text; }
    }
  }
  return null;
}

function txHash(res: RpcTransactionResponse): string {
  return (res as { transaction?: { hash?: string } }).transaction?.hash ?? res.transaction_outcome.id;
}

function nearOps(cfg: ChainConfig): ChainOps {
  const provider = new JsonRpcProvider({ url: cfg.rpcUrl });
  const game = new Account(cfg.gameAccount, provider, KeyPairSigner.fromSecretKey(cfg.privateKey));
  const relayerKey = cfg.relayerKey ?? readKey(cfg.network, cfg.relayerAccount);
  const relayer = relayerKey ? new Account(cfg.relayerAccount, provider, KeyPairSigner.fromSecretKey(relayerKey)) : null;
  const players = new Map<string, Account>();

  return {
    async createAccount(account, publicKey) {
      // Check the saved key first, including after a creation request timed out.
      try { await provider.viewAccessKey({ accountId: account, publicKey }); return; } catch { /* create below */ }
      try {
        const result = await game.createAccount({ newAccountId: account, publicKey, nearToTransfer: 50_000_000_000_000_000_000_000n });
        checkReceipts(result);
      } catch (e) {
        try { await provider.viewAccessKey({ accountId: account, publicKey }); } catch { throw e; }
      }
    },
    async createLimitedAccount(account, keys) {
      if (!keys.length) throw new Error("a limited account needs at least one key");
      try { await provider.viewAccessKey({ accountId: account, publicKey: keys[0]!.publicKey }); return; } catch { /* create below */ }
      try {
        const res = await game.signAndSendTransaction({ receiverId: account, actions: [
          actions.createAccount(),
          actions.transfer(50_000_000_000_000_000_000_000n),
          // Gas is paid by the relayer; the allowance only bounds fees if the key ever signs directly.
          ...keys.map((k) => actions.addFunctionCallAccessKey(PublicKey.fromString(k.publicKey), k.contractId, k.methods, 250_000_000_000_000_000_000_000n)),
        ] });
        checkReceipts(res);
      } catch (e) {
        try { await provider.viewAccessKey({ accountId: account, publicKey: keys[0]!.publicKey }); } catch { throw e; }
      }
    },
    async call(contractId, methodName, args, deposit = 0n) {
      const res = await game.callFunctionRaw({ contractId, methodName, args, gas: GAS, deposit });
      checkReceipts(res);
      return { hash: txHash(res), result: getTransactionLastResult(res), events: eventsIn(res) };
    },
    async relay(player, contractId, methodName, args, deposit = 0n) {
      if (!relayer) throw new Error(`no key for the relayer ${cfg.relayerAccount}`);
      // Agents hold one limited key per contract, so the signer depends on the receiving contract.
      const key = cfg.playerKey?.(player, contractId) ?? custodialKey(cfg, player);
      if (!key) throw new Error(`the server holds no key for ${player}`);
      const cacheKey = `${player}|${key}`;
      let acct = players.get(cacheKey);
      if (!acct) {
        acct = new Account(player, provider, KeyPairSigner.fromSecretKey(key));
        players.set(cacheKey, acct);
      }
      const { signedDelegate } = await acct.createSignedMetaTransaction({
        receiverId: contractId,
        actions: [actions.functionCall(methodName, args, GAS, deposit)],
      });
      const res = await relayer.signAndSendTransaction({ receiverId: player, actions: [actions.signedDelegate(signedDelegate)] });
      checkReceipts(res);
      return { hash: txHash(res), result: returnedBy(res, contractId), events: eventsIn(res) };
    },
    async view(contractId, methodName, args) {
      return provider.callFunction({ contractId, method: methodName, args });
    },
  };
}

/**
 * Demo players' keys (Phase 2 only): the server signs for its own sub-accounts (e.g. afreen.grudgebound.testnet)
 * whose keys are in NEAR CLI's keychain on this machine. Phase 3 moves player keys to the player's side.
 */
export function custodialKey(cfg: Pick<ChainConfig, "network" | "gameAccount">, player: string): KeyPairString | undefined {
  if (!isAccountId(player) || !player.endsWith(`.${cfg.gameAccount}`)) return undefined;
  return readKey(cfg.network, player);
}

export interface RuleProposal {
  /** Id for GET /chain?rule=<id>. */
  jobId: string;
  account: string;
  templateId: string;
  params: Record<string, string | number>;
  /** Kessa's verdict, decided by the server before anything is sent. */
  accepted: boolean;
  reason: string;
}

export class ChainWriter {
  /** Receipts by battle id (and "rule:<job id>"), so a client can fetch one that was still pending. */
  private readonly receipts = new Map<string, ChainReceipt>();
  /** One transaction at a time: they share the game master's key, so they must not race for a nonce. */
  private queue: Promise<unknown> = Promise.resolve();
  private readonly call: ContractCall;
  private readonly relay: RelayCall;
  private readonly view: ViewCall;
  private readonly createAccount: (account: string, publicKey: string) => Promise<void>;
  private readonly createLimitedAccount: (account: string, keys: LimitedKey[]) => Promise<void>;
  private readonly eventListeners: ((events: ChainEvent[]) => void)[] = [];

  constructor(
    readonly cfg: Pick<ChainConfig, "network" | "itemsContract" | "riteContract" | "guestAccount"> & Partial<ChainConfig>,
    ops?: ContractCall | Partial<ChainOps>,
  ) {
    const given: Partial<ChainOps> = typeof ops === "function" ? { call: ops } : ops ?? {};
    // The real connection is only built for what wasn't given (tests pass fakes and have no keys).
    let realOps: ChainOps | undefined;
    const real = () => (realOps ??= nearOps(cfg as ChainConfig));
    const call: ContractCall = given.call ?? ((...a) => real().call(...a));
    const relay: RelayCall = given.relay ?? ((...a) => real().relay(...a));
    // Every write passes its contract events on (GET /events shows them).
    this.call = async (...a) => this.emitEvents(await call(...a));
    this.relay = async (...a) => this.emitEvents(await relay(...a));
    this.view = given.view ?? ((...a) => real().view(...a));
    this.createAccount = given.createAccount ?? ((...a) => real().createAccount!(...a));
    this.createLimitedAccount = given.createLimitedAccount ?? ((...a) => real().createLimitedAccount!(...a));
  }

  /** Called with the NEP-297 events of every transaction this writer sends. */
  onEvents(listener: (events: ChainEvent[]) => void): void {
    this.eventListeners.push(listener);
  }

  private emitEvents(r: CallResult): CallResult {
    if (r.events?.length) {
      const events = r.events.map((e) => ({ ...e, txHash: r.hash, url: explorerTxUrl(this.cfg.network, r.hash) }));
      for (const l of this.eventListeners) {
        try { l(events); } catch (e) { console.warn(`chain event listener failed: ${(e as Error).message}`); }
      }
    }
    return r;
  }

  receipt(battleId: string): ChainReceipt | undefined {
    return this.receipts.get(battleId);
  }

  ruleReceipt(jobId: string): ChainReceipt | undefined {
    return this.receipts.get(`rule:${jobId}`);
  }

  /** Points on the rite contract. */
  async points(account: string): Promise<number> {
    return Number(await this.view(this.cfg.riteContract, "get_points", { account_id: account }));
  }

  /** What the next rule costs on the rite contract. */
  async ruleCost(): Promise<number> {
    return Number(await this.view(this.cfg.riteContract, "rule_cost", {}));
  }

  /** Rules in force on the rite contract. */
  async activeRules(): Promise<ChainRule[]> {
    return (await this.view(this.cfg.riteContract, "get_active_rules", {})) as ChainRule[];
  }

  async vows(account: string): Promise<ChainVow[]> {
    return await this.view(this.cfg.riteContract, "get_vows", { account_id: account }) as ChainVow[];
  }

  vowReceipt(jobId: string): ChainReceipt | undefined {
    return this.receipts.get(`vow:${jobId}`);
  }

  /**
   * The player proposes through the relayer (their points pay the cost), then the game master, as Arbiter,
   * resolves it with Kessa's verdict (a rejection refunds the cost on chain). `onResolved` runs after both.
   */
  proposeRule(p: RuleProposal, onResolved: (chainRuleId: number) => void, waitMs = 0): Promise<ChainReceipt> {
    const receipt: ChainReceipt = { status: "pending", network: this.cfg.network, account: p.account, txs: [] };
    this.remember(`rule:${p.jobId}`, receipt);
    const work = this.enqueue(async () => {
      try {
        const proposed = await this.relay(p.account, this.cfg.riteContract, "propose_rule", { template_id: p.templateId, params: JSON.stringify(p.params) });
        const ruleId = Number(proposed.result);
        if (!Number.isInteger(ruleId)) throw new Error(`propose_rule returned ${JSON.stringify(proposed.result)}`);
        receipt.ruleId = ruleId;
        receipt.txs.push({ kind: "propose", hash: proposed.hash, url: explorerTxUrl(this.cfg.network, proposed.hash) });
        const resolved = await this.call(this.cfg.riteContract, "resolve_rule", { rule_id: ruleId, accepted: p.accepted, reason: p.reason });
        receipt.txs.push({ kind: "resolve", hash: resolved.hash, url: explorerTxUrl(this.cfg.network, resolved.hash) });
        receipt.status = "done";
        onResolved(ruleId);
      } catch (e) {
        // Proposed but the verdict didn't land (or its answer was lost): settle it once more before giving up, so
        // the author's proposal slot isn't held until a restart.
        if (receipt.ruleId !== undefined && receipt.status === "pending" && await this.finishResolve(p, receipt, onResolved)) return;
        receipt.status = "failed";
        receipt.error = (e instanceof Error ? e.message : String(e)).slice(0, 300);
        console.warn(`[chain] rule ${p.jobId}: ${receipt.error}`);
      }
    });
    return this.settle(work, receipt, waitMs);
  }

  /** Reads the proposed rule back; resolves it if still Pending, or accepts a verdict that did land. */
  private async finishResolve(p: RuleProposal, receipt: ChainReceipt, onResolved: (chainRuleId: number) => void): Promise<boolean> {
    const ruleId = receipt.ruleId!;
    try {
      const [rule] = (await this.view(this.cfg.riteContract, "get_rules", { from_index: ruleId, limit: 1 })) as { id: number; status: string }[];
      if (!rule || rule.id !== ruleId) return false;
      if (rule.status === "Pending") {
        const resolved = await this.call(this.cfg.riteContract, "resolve_rule", { rule_id: ruleId, accepted: p.accepted, reason: p.reason });
        receipt.txs.push({ kind: "resolve", hash: resolved.hash, url: explorerTxUrl(this.cfg.network, resolved.hash) });
      } else if (rule.status !== (p.accepted ? "Active" : "Rejected")) {
        return false; // someone else changed it: leave it for a person to look at
      }
      receipt.status = "done";
      onResolved(ruleId);
      return true;
    } catch {
      return false;
    }
  }

  /**
   * The player repeals a rule in force through the relayer (their points pay twice the rule's cost on chain).
   * `onRepealed` runs once the contract has taken it out of force, `onFailed` if it refused.
   */
  repealRule(
    p: { jobId: string; account: string; ruleId: number }, onRepealed: () => void, onFailed: () => void = () => {}, waitMs = 0,
  ): Promise<ChainReceipt> {
    const receipt: ChainReceipt = { status: "pending", network: this.cfg.network, account: p.account, txs: [], ruleId: p.ruleId };
    this.remember(`rule:${p.jobId}`, receipt);
    const work = this.enqueue(async () => {
      try {
        const r = await this.relay(p.account, this.cfg.riteContract, "repeal_rule", { rule_id: p.ruleId });
        receipt.txs.push({ kind: "repeal", hash: r.hash, url: explorerTxUrl(this.cfg.network, r.hash) });
        receipt.status = "done";
        onRepealed();
      } catch (e) {
        receipt.status = "failed";
        receipt.error = (e instanceof Error ? e.message : String(e)).slice(0, 300);
        console.warn(`[chain] repeal ${p.jobId}: ${receipt.error}`);
        onFailed();
      }
    });
    return this.settle(work, receipt, waitMs);
  }

  /** A player's Ember on the items contract. */
  async ember(account: string): Promise<number> {
    return Number(await this.view(this.cfg.itemsContract, "mt_balance_of", { account_id: account, token_id: "ember" }));
  }

  /** A talisman on the items contract, or null. */
  async talisman(tokenId: string): Promise<{ token_id: string; owner: string; meta: { species: string; emotion: string; omen: string } } | null> {
    return (await this.view(this.cfg.itemsContract, "talisman", { token_id: tokenId })) as never;
  }

  /** Every talisman `account` owns on the items contract (pages through talismans_of). */
  async talismansOf(account: string): Promise<ChainTalisman[]> {
    const out: ChainTalisman[] = [];
    let from: number | null = 1;
    for (let pages = 0; from !== null && pages < 50; pages++) {
      const page = (await this.view(this.cfg.itemsContract, "talismans_of", { account_id: account, from_index: from, limit: 200 })) as
        { talismans: ChainTalisman[]; next_index: number | null } | null;
      if (!page) break;
      out.push(...page.talismans);
      from = page.next_index;
    }
    return out;
  }

  shopReceipt(jobId: string): ChainReceipt | undefined {
    return this.receipts.get(`shop:${jobId}`);
  }

  /**
   * Runs `steps` in the chain queue under a receipt (GET /chain?<kind>=<id>), then `onDone` if they all
   * succeed, or `onFail` if one throws. The steps push their own transactions with `tx`.
   */
  job(
    kind: "shop" | "vow" | "agent",
    id: string,
    account: string,
    steps: (ops: ChainJobOps) => Promise<void>,
    onDone: (receipt: ChainReceipt) => void,
    onFail: (receipt: ChainReceipt, ops: ChainJobOps) => void | Promise<void> = () => {},
    waitMs = 0,
  ): Promise<ChainReceipt> {
    const receipt: ChainReceipt = { status: "pending", network: this.cfg.network, account, txs: [] };
    this.remember(`${kind}:${id}`, receipt);
    const tx = (k: ChainTx["kind"], hash: string) => { receipt.txs.push({ kind: k, hash, url: explorerTxUrl(this.cfg.network, hash) }); };
    const work = this.enqueue(async () => {
      try {
        await steps({ call: this.call, relay: this.relay, tx, receipt });
        receipt.status = "done";
        onDone(receipt);
      } catch (e) {
        receipt.error = (e instanceof Error ? e.message : String(e)).slice(0, 300);
        console.warn(`[chain] ${kind} ${id}: ${receipt.error}`);
        // Recovery uses this same queue slot, so no other trade can spend the refund first.
        receipt.status = "pending";
        try { await onFail(receipt, { call: this.call, relay: this.relay, tx, receipt }); }
        catch (recovery) { receipt.error = `${receipt.error}; recovery: ${recovery instanceof Error ? recovery.message : String(recovery)}`.slice(0, 300); }
        if (receipt.status === "pending") receipt.status = "failed";
      }
    });
    return this.settle(work, receipt, waitMs);
  }

  private remember(key: string, receipt: ChainReceipt) {
    this.receipts.set(key, receipt);
    if (this.receipts.size > 2000) this.receipts.delete(this.receipts.keys().next().value!);
  }

  private settle(work: Promise<void>, receipt: ChainReceipt, waitMs: number): Promise<ChainReceipt> {
    let timer: NodeJS.Timeout | undefined;
    const timeout = new Promise<void>((resolve) => { timer = setTimeout(resolve, waitMs); });
    return Promise.race([work, timeout]).then(() => {
      clearTimeout(timer);
      return { ...receipt, txs: [...receipt.txs] };
    });
  }

  /**
   * Writes the battle on chain. Resolves within `waitMs` with the receipt so far: "done", "failed",
   * or "pending" (the writes carry on; GET /chain?battle=<id> shows the end state).
   * Each testnet transaction takes ~2.5 s, so the battle routes use waitMs = 0 and the client polls.
   */
  record(r: BattleRecord, waitMs = 0, onSettled: (receipt: ChainReceipt) => void = () => {}): Promise<ChainReceipt> {
    const receipt: ChainReceipt = { status: "pending", network: this.cfg.network, account: r.account, txs: [] };
    this.remember(r.battleId, receipt);

    const work = this.enqueue(async () => {
      try {
        if (r.outcome === "bound") {
          const meta = { species: r.wild.speciesId, emotion: r.wild.emotion, omen: r.wild.omen, level: 1, wins: 0 };
          const { hash, result } = await this.call(this.cfg.itemsContract, "mint_talisman", { to: r.account, meta });
          receipt.tokenId = String(result);
          receipt.txs.push({ kind: "mint", hash, url: explorerTxUrl(this.cfg.network, hash) });
        }
        if (r.points > 0) {
          const result = { battle_id: r.battleId, seed: r.seed, log_hash: battleLogHash(r), changes: [{ account_id: r.account, delta: r.points }] };
          const { hash } = await this.call(this.cfg.riteContract, "submit_battle", { result });
          receipt.txs.push({ kind: "points", hash, url: explorerTxUrl(this.cfg.network, hash) });
        }
        receipt.status = "done";
      } catch (e) {
        receipt.status = "failed";
        receipt.error = (e instanceof Error ? e.message : String(e)).slice(0, 300);
        console.warn(`[chain] battle ${r.battleId}: ${receipt.error}`);
      }
      // Includes partial settlement: tokenId is present if mint succeeded but the points write failed.
      onSettled(receipt);
    });
    return this.settle(work, receipt, waitMs);
  }

  private enqueue(job: () => Promise<void>): Promise<void> {
    const next = this.queue.then(job, job);
    this.queue = next.catch(() => undefined);
    return next;
  }

  /** Opt-in after the items code upgrade. The contract locks one first partner per recipient, including retries. */
  async mintStarter(account: string, species: string): Promise<{ tokenId: string; url: string }> {
    if (this.cfg.network !== "testnet") throw new Error("Starter grants are testnet-only");
    let minted: { tokenId: string; url: string } | undefined;
    await this.enqueue(async () => {
      const { result, hash } = await this.call(this.cfg.itemsContract, "mint_starter", { to: account, species });
      if (typeof result !== "string" || !result.startsWith("tal-")) throw new Error("Starter receipt could not be read");
      minted = { tokenId: result, url: explorerTxUrl(this.cfg.network, hash) };
    });
    return minted!;
  }

  /** An AI agent's account: function-call keys only, created on the game master's write queue. Testnet only. */
  provisionLimitedAccount(account: string, keys: LimitedKey[]): Promise<void> {
    if (this.cfg.network !== "testnet" || !this.cfg.gameAccount || !account.endsWith(`.${this.cfg.gameAccount}`) || !isAccountId(account)) {
      return Promise.reject(new Error("Agent provisioning is restricted to this game's testnet sub-accounts"));
    }
    return this.enqueue(() => this.createLimitedAccount(account, keys));
  }

  /** An agent chain action's receipt (GET /chain?agent=<id>). */
  agentReceipt(id: string): ChainReceipt | undefined {
    return this.receipts.get(`agent:${id}`);
  }

  /** Account funding shares the game master's write queue with battle rewards and shop settlement. */
  provisionAccount(account: string, publicKey: string): Promise<void> {
    if (this.cfg.network !== "testnet" || !this.cfg.gameAccount || !account.endsWith(`.${this.cfg.gameAccount}`) || !isAccountId(account)) {
      return Promise.reject(new Error("Player provisioning is restricted to this game's testnet sub-accounts"));
    }
    return this.enqueue(() => this.createAccount(account, publicKey));
  }
}

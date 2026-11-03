/**
 * Battle results on NEAR testnet (P2-C4).
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
 */
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { Account, JsonRpcProvider, KeyPairSigner, getTransactionLastResult, type KeyPairString } from "near-api-js";
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
}

/** One contract call: what it returned and the transaction hash. Swapped for a fake in tests. */
export type ContractCall = (contractId: string, methodName: string, args: Record<string, unknown>) => Promise<{ hash: string; result: unknown }>;

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
}

export interface ChainTx {
  kind: "mint" | "points";
  hash: string;
  url: string;
}

export interface ChainReceipt {
  status: "done" | "pending" | "failed";
  network: string;
  account: string;
  /** Set when a talisman was minted, e.g. "tal-3". */
  tokenId?: string;
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
export function battleLogHash(r: Pick<BattleRecord, "seed" | "actions" | "wild" | "playerCurse">): string {
  const log = JSON.stringify({ seed: r.seed, wild: r.wild.speciesId, omen: r.wild.omen, playerCurse: r.playerCurse, actions: r.actions });
  return createHash("sha256").update(log).digest("hex");
}

function nearCall(cfg: ChainConfig): ContractCall {
  const provider = new JsonRpcProvider({ url: cfg.rpcUrl });
  const account = new Account(cfg.gameAccount, provider, KeyPairSigner.fromSecretKey(cfg.privateKey));
  return async (contractId, methodName, args) => {
    const res = await account.callFunctionRaw({ contractId, methodName, args, gas: 30_000_000_000_000n, deposit: 0n });
    const hash = (res as { transaction?: { hash?: string } }).transaction?.hash ?? res.transaction_outcome.id;
    return { hash, result: getTransactionLastResult(res) };
  };
}

export class ChainWriter {
  /** Receipts by battle id, so a client can fetch one that was still pending when the battle ended. */
  private readonly receipts = new Map<string, ChainReceipt>();
  /** One transaction at a time: they share the game master's key, so they must not race for a nonce. */
  private queue: Promise<unknown> = Promise.resolve();
  private readonly call: ContractCall;

  constructor(readonly cfg: Pick<ChainConfig, "network" | "itemsContract" | "riteContract" | "guestAccount"> & Partial<ChainConfig>, call?: ContractCall) {
    this.call = call ?? nearCall(cfg as ChainConfig);
  }

  receipt(battleId: string): ChainReceipt | undefined {
    return this.receipts.get(battleId);
  }

  /**
   * Writes the battle on chain. Resolves within `waitMs` with the receipt so far: "done", "failed",
   * or "pending" (the writes carry on; GET /chain?battle=<id> shows the end state).
   * Each testnet transaction takes ~2.5 s, so the battle routes use waitMs = 0 and the client polls.
   */
  record(r: BattleRecord, waitMs = 0): Promise<ChainReceipt> {
    const receipt: ChainReceipt = { status: "pending", network: this.cfg.network, account: r.account, txs: [] };
    this.receipts.set(r.battleId, receipt);
    if (this.receipts.size > 2000) this.receipts.delete(this.receipts.keys().next().value!);

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
    });

    let timer: NodeJS.Timeout | undefined;
    const timeout = new Promise<void>((resolve) => { timer = setTimeout(resolve, waitMs); });
    return Promise.race([work, timeout]).then(() => {
      clearTimeout(timer);
      return { ...receipt, txs: [...receipt.txs] };
    });
  }

  private enqueue(job: () => Promise<void>): Promise<void> {
    const next = this.queue.then(job, job);
    this.queue = next.catch(() => undefined);
    return next;
  }
}

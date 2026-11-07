/**
 * Players trading with merchants, settled in Ember and talismans on NEAR (P2-G1).
 *
 *   POST /shop/buy  { account, merchant, itemId }  -> a talisman of that emotion: the player's Ember goes to the
 *        game master (relayed mt_transfer; the game master holds the merchants' purses), then a talisman is minted
 *        to the player. The merchant's stock and demand change at once in the world, so the price moves for everyone.
 *   POST /shop/sell { account, merchant, tokenId } -> the player hands the talisman to the game master (relayed),
 *        and the game master pays the merchant's buy-back price in Ember (less any trade tax rule, R7).
 *   GET /chain?shop=<jobId> -> the receipt.
 *
 * Supplies use the same stock/price and Ember settlement; their counts are stored in the server Satchel.
 * Merchants are simulated (their purse is world.merchants[].ember); they get NEAR accounts in Phase 3.
 */
import { randomUUID } from "node:crypto";
import {
  CURSES, curseById, log, payTradeTax, quoteBuyPrice, quoteSellPrice, recordPurchase, recordSale, totalGrudge,
  type Merchant, type World,
} from "@grudgebound/shared";
import { isAccountId, type ChainReceipt, type ChainWriter } from "./chain";
import { displayName } from "./rules";
import { Satchels, supplyForItem } from "./satchel";

type Answer<T> = { status: 200; body: T } | { status: number; body: { error: string } };

export interface TradeAnswer {
  jobId: string;
  /** Ember paid (buy) or received after tax (sell). */
  price: number;
  /** What changed hands, in words. */
  text: string;
  /** The talisman's curse (bought: the one sealed for you; sold: the one handed over). */
  speciesId: string;
  omen: string;
  chain: ChainReceipt;
  /** Present for server-held supplies (speciesId/omen stay empty for the legacy trade parser). */
  itemId?: string;
}

/** Extra numbers for the shop screen, merged into GET /shop. */
export interface ShopExtras {
  /** The player's Ember on chain; null off-chain or without an account. */
  ember: number | null;
  /** What this merchant pays for a talisman of each emotion it trades (talisman-rage -> 18). */
  buyBack: Record<string, number>;
}

/** The game master tops its Ember treasury up by this much when a payout would overdraw it. */
const TREASURY_TOP_UP = 1000;
const ONE_YOCTO = 1n;

function fail(status: number, error: string): { status: number; body: { error: string } } {
  return { status, body: { error } };
}

function ctxFor(world: World, m: Merchant) {
  return { wardGrudge: totalGrudge(world.wards[m.ward].grudge), buyerIsOathbreaker: false };
}

export class Shop {
  private readonly pending = new Set<string>();
  isTrading(account: string) { return this.pending.has(account); }
  constructor(
    private readonly world: World,
    private readonly chain: ChainWriter,
    private readonly gameAccount: string,
    private readonly canSignFor: (account: string) => boolean,
    /** Picks the species a bought talisman holds (tests pass a fixed one). */
    private readonly pick: <T>(xs: readonly T[]) => T = (xs) => xs[Math.floor(Math.random() * xs.length)]!,
    private readonly satchels: Satchels = new Satchels(world, chain),
  ) {}

  private merchant(id: unknown): Merchant | undefined {
    return typeof id === "string" ? this.world.merchants.find((m) => m.id === id) : undefined;
  }

  private checkAccount(account: unknown): string | { status: number; body: { error: string } } {
    if (!isAccountId(account)) return fail(400, "account must be a NEAR account id");
    if (!this.canSignFor(account)) return fail(403, `the server can't sign for ${account} (only its demo players until email sign-in)`);
    return account;
  }

  async extras(merchantId: string, account: string | null): Promise<ShopExtras> {
    const m = this.merchant(merchantId);
    const buyBack: Record<string, number> = {};
    if (m) {
      for (const it of m.items) if (it.id.startsWith("talisman-")) buyBack[it.id] = quoteBuyPrice(it, ctxFor(this.world, m)).price;
    }
    let ember: number | null = null;
    if (account && isAccountId(account)) {
      try { ember = await this.chain.ember(account); } catch { ember = null; }
    }
    return { ember, buyBack };
  }

  async buy(body: unknown): Promise<Answer<TradeAnswer>> {
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    const account = this.checkAccount(b.account);
    if (typeof account !== "string") return account;
    if (this.isTrading(account)) return fail(409, "Wait for your previous trade's receipt.");
    const m = this.merchant(b.merchant);
    if (!m) return fail(404, "no such merchant");
    if (m.closed) return fail(409, `${m.name} has shut up shop`);
    const itemId = m.id === "broker" && b.itemId === "binding-seal" ? "blank-talisman" : b.itemId;
    const idx = m.items.findIndex((i) => i.id === itemId);
    const item = m.items[idx];
    if (!item) return fail(404, `${m.name} doesn't sell that`);
    const supply = supplyForItem(item.id);
    if (!item.id.startsWith("talisman-") && !supply) return fail(400, "that item is not sold to players");
    if (item.stock <= 0) return fail(409, `${m.name} is out of ${item.name.toLowerCase()}`);

    const price = quoteSellPrice(item, ctxFor(this.world, m)).price;
    const emotion = item.id.slice("talisman-".length);
    const pool = CURSES.filter((c) => c.emotion === emotion && c.omen === "whisper");
    if (!supply && !pool.length) return fail(500, `no ${emotion} curse to seal`);
    const species = supply ? null : this.pick(pool);

    // The merchant's side changes now, so a second buyer sees the new price and stock.
    this.pending.add(account);
    m.items[idx] = recordSale(item);
    m.ember += price;
    const rollback = () => {
      m.items[idx] = { ...m.items[idx]!, stock: m.items[idx]!.stock + 1, buys24h: Math.max(0, m.items[idx]!.buys24h - 1) };
      m.ember -= price;
      this.pending.delete(account);
    };
    let ember: number;
    try { ember = await this.chain.ember(account); } catch (e) { rollback(); return fail(502, `could not read the items contract: ${(e as Error).message}`); }
    if (ember < price) { rollback(); return fail(402, `${item.name} cost ${price} Ember; ${displayName(account)} has ${ember}.`); }
    const jobId = randomUUID();
    const text = `${displayName(account)} bought ${species ? `a ${species.name} talisman` : item.name.toLowerCase()} from ${m.name} for ${price} Ember`;
    const chain = await this.chain.job("shop", jobId, account, async ({ call, relay, tx, receipt }) => {
      const paid = await relay(account, this.chain.cfg.itemsContract, "mt_transfer",
        { receiver_id: this.gameAccount, token_id: "ember", amount: String(price), memo: `buy ${item.id} from ${m.id}` }, ONE_YOCTO);
      tx("pay", paid.hash);
      if (species) {
        const meta = { species: species.id, emotion: species.emotion, omen: species.omen, level: 1, wins: 0 };
        const minted = await call(this.chain.cfg.itemsContract, "mint_talisman", { to: account, meta });
        receipt.tokenId = String(minted.result);
        tx("mint", minted.hash);
      }
    }, () => {
      try {
        if (supply) this.satchels.addSupply(account, supply);
        log(this.world, { type: "trade", ward: m.ward, actor: account, text: `${text}.` });
      } finally { this.pending.delete(account); }
    }, rollback);
    return { status: 200, body: { jobId, price, text, speciesId: species?.id ?? "", omen: species?.omen ?? "", itemId: item.id, chain } };
  }

  async sell(body: unknown): Promise<Answer<TradeAnswer>> {
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    const account = this.checkAccount(b.account);
    if (typeof account !== "string") return account;
    if (this.isTrading(account)) return fail(409, "Wait for your previous trade's receipt.");
    const m = this.merchant(b.merchant);
    if (!m) return fail(404, "no such merchant");
    if (m.closed) return fail(409, `${m.name} has shut up shop`);
    const tokenId = b.tokenId;
    if (typeof tokenId !== "string" || !tokenId) return fail(400, "tokenId must be a talisman token id");

    let tal;
    try { tal = await this.chain.talisman(tokenId); } catch (e) { return fail(502, `could not read the items contract: ${(e as Error).message}`); }
    if (!tal) return fail(404, `no talisman ${tokenId}`);
    if (tal.owner !== account) return fail(403, `${tokenId} belongs to ${tal.owner}`);
    if (this.isTrading(account)) return fail(409, "Wait for your previous trade's receipt.");
    const idx = m.items.findIndex((i) => i.id === `talisman-${tal.meta.emotion}`);
    const item = m.items[idx];
    if (!item) return fail(409, `${m.name} doesn't buy ${tal.meta.emotion} talismans`);
    const offer = quoteBuyPrice(item, ctxFor(this.world, m)).price;
    if (m.ember < offer) return fail(409, `${m.name} can't afford it today`);

    this.pending.add(account);
    m.ember -= offer;
    m.items[idx] = recordPurchase(item);
    const paid = payTradeTax(this.world, offer);
    let name = tal.meta.species;
    try { name = curseById(tal.meta.species).name; } catch { /* keep the id */ }
    const jobId = randomUUID();
    const text = `${displayName(account)} sold a ${name} talisman (${tokenId}) to ${m.name} for ${paid} Ember`;
    const game = this.gameAccount;
    const items = this.chain.cfg.itemsContract;
    const chain = await this.chain.job("shop", jobId, account, async ({ call, relay, tx, receipt }) => {
      receipt.tokenId = tokenId;
      const handed = await relay(account, items, "mt_transfer", { receiver_id: game, token_id: tokenId, amount: "1", memo: `sell to ${m.id}` }, ONE_YOCTO);
      tx("hand-over", handed.hash);
      if (paid > 0) {
        if ((await this.chain.ember(game)) < paid) await call(items, "mint_ember", { to: game, amount: String(TREASURY_TOP_UP) });
        const sent = await call(items, "mt_transfer", { receiver_id: account, token_id: "ember", amount: String(paid), memo: `${m.id} buys ${tokenId}` }, ONE_YOCTO);
        tx("pay", sent.hash);
      }
    }, () => {
      this.pending.delete(account);
      log(this.world, { type: "trade", ward: m.ward, actor: account, text: `${text}.` });
    }, () => {
      this.pending.delete(account);
      m.ember += offer;
      m.items[idx] = { ...m.items[idx]!, stock: Math.max(0, m.items[idx]!.stock - 1) };
    });
    return { status: 200, body: { jobId, price: paid, text, speciesId: tal.meta.species, omen: tal.meta.omen, chain } };
  }
}

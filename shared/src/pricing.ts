/**
 * ============================================================================
 *  pricing.ts: EVERY price formula in Grudgebound lives in this one file.
 * ============================================================================
 *
 *  price = base
 *        x clamp( sqrt(targetStock / max(stock, 1)), 0.4, 2.5 )    // scarcity
 *        x ( 1 + 0.3 * (buys24h - sells24h) / targetStock )         // recent demand
 *        x ( 1 + 0.005 * wardGrudge )   // fear: only for protective goods
 *        x ( oathbreaker ? 1.25 : 1 )   // merchants distrust vow breakers
 *
 *  Merchants BUY from players at BUYBACK_RATIO of their selling price.
 *  A merchant whose Ember stays below BANKRUPT_EMBER for BANKRUPT_DAYS days closes.
 *
 *  Every quote returns the factors and a plain-English reason, which the UI shows
 *  in the merchant's tooltip. The AI may reword the reason in character, but it
 *  never changes the number.
 */

export const SCARCITY_MIN = 0.4;
export const SCARCITY_MAX = 2.5;
export const DEMAND_WEIGHT = 0.3;
export const FEAR_WEIGHT = 0.005;
export const OATHBREAKER_MARKUP = 1.25;
export const BUYBACK_RATIO = 0.6;
export const BANKRUPT_EMBER = 50;
export const BANKRUPT_DAYS = 2;

export interface MarketItem {
  id: string;
  name: string;
  base: number;
  stock: number;
  targetStock: number;
  buys24h: number;
  sells24h: number;
  /** Charms and wards: fear (Grudge) makes them pricier. */
  protective: boolean;
}

export interface PriceContext {
  wardGrudge: number;
  buyerIsOathbreaker: boolean;
}

export interface PriceFactors {
  scarcity: number;
  demand: number;
  fear: number;
  distrust: number;
}

export interface PriceQuote {
  itemId: string;
  price: number;
  factors: PriceFactors;
  reason: string;
}

const clamp = (x: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, x));

export function priceFactors(item: MarketItem, ctx: PriceContext): PriceFactors {
  const scarcity = clamp(Math.sqrt(item.targetStock / Math.max(item.stock, 1)), SCARCITY_MIN, SCARCITY_MAX);
  const demand = Math.max(0.5, 1 + (DEMAND_WEIGHT * (item.buys24h - item.sells24h)) / Math.max(1, item.targetStock));
  const fear = item.protective ? 1 + FEAR_WEIGHT * ctx.wardGrudge : 1;
  const distrust = ctx.buyerIsOathbreaker ? OATHBREAKER_MARKUP : 1;
  return { scarcity, demand, fear, distrust };
}

/** What the merchant SELLS the item for. */
export function quoteSellPrice(item: MarketItem, ctx: PriceContext): PriceQuote {
  const f = priceFactors(item, ctx);
  const price = Math.max(1, Math.round(item.base * f.scarcity * f.demand * f.fear * f.distrust));
  return { itemId: item.id, price, factors: f, reason: explain(item, f) };
}

/** What the merchant PAYS a player for the item. */
export function quoteBuyPrice(item: MarketItem, ctx: PriceContext): PriceQuote {
  const sell = quoteSellPrice(item, { ...ctx, buyerIsOathbreaker: false });
  const price = Math.max(1, Math.floor(sell.price * BUYBACK_RATIO));
  return { ...sell, price, reason: sell.reason };
}

/** One-line reason for the tooltip, naming the strongest factor. */
export function explain(item: MarketItem, f: PriceFactors): string {
  const parts: string[] = [];
  // Item names are plural ("Ward charms", "Sorrow talismans").
  if (f.scarcity >= 1.3) parts.push(`only ${item.stock} ${item.name.toLowerCase()} left`);
  else if (f.scarcity <= 0.8) parts.push(`${item.name.toLowerCase()} are flooding in (${item.stock} in stock)`);
  if (f.demand >= 1.15) parts.push("everyone has been buying it today");
  else if (f.demand <= 0.85) parts.push("people have been selling it off");
  if (f.fear >= 1.15) parts.push("the Ward is afraid, and fear sells charms");
  if (f.distrust > 1) parts.push("you broke a vow, so it costs you extra");
  if (parts.length === 0) return `${item.name} are at a fair price today.`;
  const text = parts.join(", and ");
  return text.charAt(0).toUpperCase() + text.slice(1) + ".";
}

/** Updates stock and demand counters after a sale to a customer. */
export function recordSale(item: MarketItem, qty = 1): MarketItem {
  return { ...item, stock: Math.max(0, item.stock - qty), buys24h: item.buys24h + qty };
}

/** Updates stock and counters after the merchant buys from someone. */
export function recordPurchase(item: MarketItem, qty = 1): MarketItem {
  return { ...item, stock: item.stock + qty, sells24h: item.sells24h + qty };
}

/** Counters roll over once per in-game day. */
export function rollDay(item: MarketItem): MarketItem {
  return { ...item, buys24h: 0, sells24h: 0 };
}

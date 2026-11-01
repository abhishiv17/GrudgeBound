/**
 * Merchant agent: prices always come from shared/pricing.ts.
 * The AI only rewrites the tooltip reason in the merchant's voice. It never changes the number.
 */
import { quoteSellPrice, totalGrudge, type World, type Merchant, type PriceQuote } from "@grudgebound/shared";
import { chat, type LlmConfig } from "./nearai";

const VOICES: Record<Merchant["role"], string> = {
  broker: "the Talisman Broker, a polite, calculating dealer in bound curses",
  apothecary: "the Apothecary, a nervous healer who sells salves and charms",
  fence: "the Fence, a shady dockside buyer who asks no questions",
};

export interface Tooltip {
  price: number;
  text: string;
  source: "llm" | "formula";
}

export async function priceTooltip(world: World, merchant: Merchant, itemId: string, llm: LlmConfig | null): Promise<Tooltip> {
  const item = merchant.items.find((i) => i.id === itemId);
  if (!item) throw new Error(`${merchant.name} does not sell ${itemId}`);
  const quote: PriceQuote = quoteSellPrice(item, { wardGrudge: totalGrudge(world.wards[merchant.ward].grudge), buyerIsOathbreaker: false });
  const plain: Tooltip = { price: quote.price, text: quote.reason, source: "formula" };
  if (!llm) return plain;
  try {
    const text = await chat(llm, [
      { role: "system", content: `You are ${VOICES[merchant.role]}. Say ONE short sentence (max 25 words) to a customer explaining this price, in character. Do not mention any number other than the price given.` },
      { role: "user", content: `Item: ${item.name}. Price: ${quote.price} Ember. Why: ${quote.reason}` },
    ], 80);
    return { price: quote.price, text: text.trim().replace(/^"|"$/g, ""), source: "llm" };
  } catch {
    return plain;
  }
}

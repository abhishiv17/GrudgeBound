/**
 * pnpm agent:demo
 * Runs two in-game days, then asks each AI rival what it wants to do next and asks
 * the Talisman Broker to explain a price. Uses NEAR AI Cloud if .env has a key,
 * otherwise the scripted fallback.
 */
import { createWorld, createRng, step, TICKS_PER_DAY } from "@grudgebound/shared";
import { llmConfigFromEnv } from "./nearai";
import { decide } from "./runtime";
import { priceTooltip } from "./merchant";

const llm = llmConfigFromEnv();
console.log(llm ? `Using NEAR AI model ${llm.model}` : "No NEAR_AI_* settings in .env: using scripted fallback.");

const world = createWorld(42);
const rng = createRng(42);
for (let i = 0; i < TICKS_PER_DAY * 2; i++) step(world, rng);

console.log(`\nDay ${Math.floor(world.tick / TICKS_PER_DAY) + 1}. What do the rivals want to do?\n`);
for (const me of world.participants.filter((p) => p.kind === "agent")) {
  const d = await decide(world, me, rng, llm);
  console.log(`  ${me.name.padEnd(15)} [${d.source}] ${d.reason}`);
}

const broker = world.merchants.find((m) => m.id === "broker")!;
const tip = await priceTooltip(world, broker, "talisman-sorrow", llm);
console.log(`\nTooltip at ${broker.name}: ${tip.price} Ember. "${tip.text}" [${tip.source}]\n`);

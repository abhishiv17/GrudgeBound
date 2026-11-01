/**
 * Fast-forward: simulate N in-game days and print how the economy moved.
 *
 *   pnpm sim                      -> 7 days, seed 42
 *   pnpm sim --days 3 --seed 7
 *   pnpm sim --events             -> also print every notable event
 *   pnpm sim --json               -> machine-readable daily summaries
 */
import { createWorld, createRng, step, summarize, TICKS_PER_DAY, type WorldSummary } from "@grudgebound/shared";

function arg(name: string, fallback: number): number {
  const i = process.argv.indexOf(`--${name}`);
  const v = i >= 0 ? Number(process.argv[i + 1]) : NaN;
  return Number.isFinite(v) ? v : fallback;
}
const flag = (name: string) => process.argv.includes(`--${name}`);

const days = arg("days", 7);
const seed = arg("seed", 42);
const showEvents = flag("events");
const asJson = flag("json");

const world = createWorld(seed);
const rng = createRng(seed);
const daily: WorldSummary[] = [];
const started = Date.now();
let eventCursor = 0;

const pad = (s: string | number, n: number) => String(s).padEnd(n);
const num = (x: number, n: number) => String(Math.round(x)).padStart(n);

if (!asJson) console.log(`\nGrudgebound fast-forward | seed ${seed} | ${days} day${days === 1 ? "" : "s"} (${days * TICKS_PER_DAY} ticks)\n`);

for (let d = 1; d <= days; d++) {
  for (let h = 0; h < TICKS_PER_DAY; h++) step(world, rng);
  const s = summarize(world);
  daily.push(s);
  if (asJson) continue;

  const newEvents = world.events.slice(eventCursor);
  eventCursor = world.events.length;

  console.log(`=== Day ${d} ${"=".repeat(60)}`);
  console.log("  Grudge      total   rage sorrow  dread   wild curses");
  for (const w of s.wards) {
    console.log(`  ${pad(w.name, 11)} ${num(w.total, 5)}  ${num(w.grudge.rage, 5)} ${num(w.grudge.sorrow, 6)} ${num(w.grudge.dread, 6)}   ${w.wild}`);
  }
  console.log("  Merchants");
  for (const m of s.merchants) {
    const prices = Object.entries(m.prices).map(([k, v]) => `${k}=${v}`).join(" ");
    console.log(`  ${pad(m.name, 20)} ${m.closed ? "CLOSED " : "open   "} Ember ${num(m.ember, 5)}   ${prices}`);
  }
  console.log("  Top points");
  for (const p of s.leaderboard.slice(0, 5)) {
    console.log(`  ${pad(p.name, 16)} ${pad(p.kind, 6)} ${num(p.points, 5)} pts  ${num(p.ember, 4)} Ember  ${p.talismans} talismans`);
  }
  const highlights = newEvents.filter((e) => ["rule-added", "rule-rejected", "bankruptcy", "reopen", "calamity", "scam"].includes(e.type));
  for (const e of showEvents ? newEvents : highlights) console.log(`  [${String(e.hour).padStart(2, "0")}:00] ${e.text}`);
  console.log("");
}

if (asJson) {
  console.log(JSON.stringify({ seed, days, daily }, null, 2));
} else {
  const last = daily[daily.length - 1]!;
  console.log(`Rules in force at the end (${last.rules.length}):`);
  for (const r of last.rules) console.log(`  #${r.id} ${r.text}  (by ${r.author}, ${r.cost} pts)`);
  console.log(`\nEmber burned by taxes: ${last.burnedEmber}`);
  console.log(`Simulated ${days} days in ${Date.now() - started} ms.\n`);
}

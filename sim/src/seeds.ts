/**
 * Demo seed finder (P2-A1): which seeds make a 7-day run show a rule war, a bankruptcy and a Calamity?
 *
 *   npm run sim:seeds                      -> seeds 1-200, 7 days
 *   npm run sim:seeds -- --from 1 --to 500 --days 7
 *
 * A "rule war" is at least 2 rules made law and at least 1 rejected for contradicting one in force.
 * Seeds are ranked so the three beats are spread out (better to watch than everything on day 1).
 */
import { createWorld, createRng, step, TICKS_PER_DAY, type GameEvent } from "@grudgebound/shared";

export interface SeedReport {
  seed: number;
  rulesAdded: number;
  rulesRejected: number;
  bankruptcies: number;
  calamities: number;
  /** Day (1-based) each beat first happens; null if never. */
  firstRuleWar: number | null;
  firstBankruptcy: number | null;
  firstCalamity: number | null;
  /** All three beats happened. */
  complete: boolean;
  /** Higher is a better demo: complete, beats spread over different days, not too early. */
  score: number;
}

const firstDay = (events: GameEvent[], type: string, nth = 1) => {
  const hits = events.filter((e) => e.type === type);
  return hits.length >= nth ? hits[nth - 1]!.day : null;
};

export function scoreSeed(seed: number, days = 7): SeedReport {
  const world = createWorld(seed);
  const rng = createRng(seed);
  for (let t = 0; t < days * TICKS_PER_DAY; t++) step(world, rng);
  const ev = world.events;
  const rulesAdded = ev.filter((e) => e.type === "rule-added").length;
  const rulesRejected = ev.filter((e) => e.type === "rule-rejected").length;
  const bankruptcies = ev.filter((e) => e.type === "bankruptcy").length;
  const calamities = ev.filter((e) => e.type === "calamity").length;
  const secondRule = firstDay(ev, "rule-added", 2);
  const firstReject = firstDay(ev, "rule-rejected");
  const firstRuleWar = secondRule !== null && firstReject !== null ? Math.max(secondRule, firstReject) : null;
  const firstBankruptcy = firstDay(ev, "bankruptcy");
  const firstCalamity = firstDay(ev, "calamity");
  const complete = firstRuleWar !== null && firstBankruptcy !== null && firstCalamity !== null;
  const beats = [firstRuleWar, firstBankruptcy, firstCalamity].filter((d): d is number => d !== null);
  const spread = new Set(beats).size;
  const notDayOne = beats.filter((d) => d > 1).length;
  // Too much of anything drowns the story: a little penalty past a handful.
  const noise = Math.max(0, bankruptcies - 2) + Math.max(0, calamities - 3);
  const score = (complete ? 100 : 0) + spread * 10 + notDayOne * 5 - noise * 3;
  return { seed, rulesAdded, rulesRejected, bankruptcies, calamities, firstRuleWar, firstBankruptcy, firstCalamity, complete, score };
}

function arg(name: string, fallback: number): number {
  const i = process.argv.indexOf(`--${name}`);
  const v = i >= 0 ? Number(process.argv[i + 1]) : NaN;
  return Number.isFinite(v) ? v : fallback;
}

if (process.argv[1] && /seeds\.ts$/.test(process.argv[1])) {
  const from = arg("from", 1), to = arg("to", 200), days = arg("days", 7);
  const reports: SeedReport[] = [];
  for (let s = from; s <= to; s++) reports.push(scoreSeed(s, days));
  const complete = reports.filter((r) => r.complete);
  const pct = (n: number) => `${Math.round((100 * n) / reports.length)}%`;
  const has = (k: keyof SeedReport) => reports.filter((r) => r[k] !== null).length;
  console.log(`\nSeeds ${from}-${to}, ${days} days: all three beats in ${complete.length} (${pct(complete.length)})`);
  console.log(`  rule war ${pct(has("firstRuleWar"))}, bankruptcy ${pct(has("firstBankruptcy"))}, Calamity ${pct(has("firstCalamity"))}\n`);
  console.log("  seed   score  rules +/-  bankrupt  calamity   first: war  bankrupt  calamity");
  for (const r of [...reports].sort((a, b) => b.score - a.score).slice(0, 12)) {
    const d = (x: number | null) => (x === null ? "  -" : `d${x}`).padStart(5);
    console.log(`  ${String(r.seed).padStart(4)}   ${String(r.score).padStart(4)}   ${String(r.rulesAdded).padStart(3)}/${String(r.rulesRejected).padEnd(3)}  ${String(r.bankruptcies).padStart(5)}    ${String(r.calamities).padStart(5)}        ${d(r.firstRuleWar)}     ${d(r.firstBankruptcy)}     ${d(r.firstCalamity)}`);
  }
  console.log("");
}

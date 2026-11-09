/** Read-only history for the native Observatory. No chain, identity or persistence dependencies. */
import { DEMO_SEED, simulate, summarize, renderRule, type World, type GameEvent } from "@grudgebound/shared";
import { displayName } from "./rules";

export interface ObservatorySample {
  tick: number; day: number; hour: number;
  wards: { id: string; rage: number; sorrow: number; dread: number; total: number }[];
  prices: Record<string, Record<string, number>>; closed: string[];
}
export interface ObservatoryRule {
  id: string; text: string; author: string; authorKind: "agent" | "player"; tick: number;
  status: "active" | "rejected" | "repealed"; url?: string; reason?: string;
}
export interface ObservatoryView {
  mode: "live" | "preview"; seed: number; samples: ObservatorySample[]; rules: ObservatoryRule[];
  events: { tick: number; type: string; ward?: string; text: string }[];
}
const MILESTONES = new Set(["bankruptcy", "calamity", "rule-added", "rule-rejected", "rule-repealed"]);
const LIMIT = 168;

export class Observatory {
  private readonly samples: ObservatorySample[] = [];
  private readonly rules = new Map<string, ObservatoryRule>();
  private readonly seen = new WeakSet<GameEvent>();
  private rejected = 0;
  constructor(private readonly world: World) {}

  sample(): void {
    const s = summarize(this.world);
    this.samples.push({ tick: s.tick, day: s.day, hour: s.hour,
      wards: s.wards.map((w) => ({ id: w.id, ...w.grudge, total: w.total })),
      prices: Object.fromEntries(s.merchants.map((m) => [m.id, { ...m.prices }])),
      closed: s.merchants.filter((m) => m.closed).map((m) => m.id) });
    if (this.samples.length > LIMIT) this.samples.shift();
    this.collectRules();
  }

  private author(id: string) {
    const p = this.world.participants.find((p) => p.id === id || id.startsWith(`agent-${p.id}.`));
    return { author: p?.name ?? displayName(id), authorKind: p?.kind === "agent" ? "agent" as const : "player" as const };
  }

  private collectRules() {
    // Existing engine events carry readable text, not structured rejected/repealed rule metadata.
    for (const e of this.world.events) {
      if (this.seen.has(e)) continue;
      this.seen.add(e);
      const number = /(?:New rule|Rule) #(\d+)/.exec(e.text)?.[1];
      if (e.type === "rule-added" && number) {
        const text = /New rule #\d+: (.*)\."$/.exec(e.text)?.[1] ?? e.text;
        this.rules.set(number, { id: number, text, ...this.author(e.actor ?? ""), tick: e.tick, status: "active", ...(e.url ? { url: e.url } : {}) });
      } else if (e.type === "rule-repealed" && number) {
        const old = this.rules.get(number);
        if (old) this.rules.set(number, { ...old, tick: e.tick, status: "repealed", ...(e.url ? { url: e.url } : {}) });
      } else if (e.type === "rule-rejected") {
        const match = /'s rule "(.*)": (.*)$/.exec(e.text);
        const id = `rejected-${++this.rejected}`;
        this.rules.set(id, { id, text: match?.[1] ?? e.text, ...this.author(e.actor ?? ""), tick: e.tick, status: "rejected",
          ...(match ? { reason: match[2] } : {}), ...(e.url ? { url: e.url } : {}) });
      }
    }
    // Also covers a chain snapshot restored before this history started.
    for (const r of this.world.rules) {
      const id = String(r.id);
      if (!this.rules.has(id)) this.rules.set(id, { id, text: renderRule(r.templateId, r.params), ...this.author(r.author), tick: r.addedAtTick, status: "active" });
    }
    while (this.rules.size > 2000) this.rules.delete(this.rules.keys().next().value!);
  }

  view(mode: "live" | "preview" = "live"): ObservatoryView {
    this.collectRules();
    return structuredClone({ mode, seed: this.world.seed, samples: this.samples,
      rules: [...this.rules.values()].sort((a, b) => b.tick - a.tick),
      events: this.world.events.filter((e) => MILESTONES.has(e.type)).map(({ tick, type, ward, text }) => ({ tick, type, ward, text })) });
  }
}

let cachedPreview: ObservatoryView | undefined;
export function observatoryPreview(): ObservatoryView {
  if (!cachedPreview) {
    let history: Observatory | undefined;
    simulate(DEMO_SEED, LIMIT, (world) => { history ??= new Observatory(world); history.sample(); });
    cachedPreview = history!.view("preview");
  }
  return structuredClone(cachedPreview);
}

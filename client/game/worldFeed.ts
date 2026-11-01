/**
 * Where the client gets the living world from:
 *  - "server": the game server (npm run server), polled every few seconds;
 *  - "offline": if no server is running, the same shared engine runs in the browser,
 *    sped up so you can watch the town change.
 * Call stop() when the scene is destroyed so no timers are left running.
 *
 * report() tells the town about a battle the player fought: the server replays it from its seed
 * and actions before believing it; the offline town just records it.
 */
import {
  createWorld, createRng, recordCapture, step, summarize,
  type CaptureReport, type GameEvent, type Omen, type PlayerAction, type WorldSummary,
} from "@grudgebound/shared";

const SERVER = process.env.NEXT_PUBLIC_SERVER_URL ?? "http://localhost:8787";
const POLL_MS = 3000;
const OFFLINE_TICK_MS = 2000;

/** Everything needed to replay a battle: setup, seed and the player's actions. */
export interface BattleProof {
  wild: string;
  omen: Omen;
  playerCurse: string;
  seed: number;
  actions: PlayerAction[];
}

/** The town's answer to a reported capture: points under the rules in force, or null if not accepted. */
export type ReportAnswer = { points: number } | { points: null; error: string };

export interface WorldFeed {
  mode: "server" | "offline";
  latest: WorldSummary;
  report(capture: CaptureReport, proof: BattleProof): Promise<ReportAnswer>;
  stop(): void;
}

export async function startWorldFeed(onUpdate: (s: WorldSummary) => void, onEvent: (e: GameEvent) => void): Promise<WorldFeed> {
  try {
    const first = await fetchJson<WorldSummary>(`${SERVER}/state`);
    let lastTick = first.tick;
    onUpdate(first);
    const timer = setInterval(async () => {
      try {
        const [s, events] = await Promise.all([
          fetchJson<WorldSummary>(`${SERVER}/state`),
          fetchJson<GameEvent[]>(`${SERVER}/events?since=${lastTick - 1}`),
        ]);
        for (const e of events) if (e.tick >= lastTick) onEvent(e);
        lastTick = s.tick;
        feed.latest = s;
        onUpdate(s);
      } catch {
        /* server hiccup: keep the last state */
      }
    }, POLL_MS);
    const feed: WorldFeed = {
      mode: "server",
      latest: first,
      async report(capture, proof) {
        try {
          return await postJson<ReportAnswer>(`${SERVER}/battle`, { ...proof, ward: capture.ward, by: capture.by, outcome: capture.verb === "bind" ? "bound" : "won" });
        } catch (e) {
          return { points: null, error: e instanceof Error ? e.message : "the town did not answer" };
        }
      },
      stop: () => clearInterval(timer),
    };
    return feed;
  } catch {
    const seed = 42;
    const world = createWorld(seed);
    const rng = createRng(seed);
    let cursor = 0;
    const timer = setInterval(() => {
      step(world, rng);
      for (const e of world.events.slice(cursor)) onEvent(e);
      cursor = world.events.length;
      feed.latest = summarize(world);
      onUpdate(feed.latest);
    }, OFFLINE_TICK_MS);
    const feed: WorldFeed = {
      mode: "offline",
      latest: summarize(world),
      async report(capture) {
        const points = recordCapture(world, capture);
        for (const e of world.events.slice(cursor)) onEvent(e);
        cursor = world.events.length;
        feed.latest = summarize(world);
        onUpdate(feed.latest);
        return { points };
      },
      stop: () => clearInterval(timer),
    };
    onUpdate(feed.latest);
    return feed;
  }
}

async function postJson<T>(url: string, body: unknown): Promise<T> {
  const res = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  const data = (await res.json().catch(() => ({}))) as T & { error?: string };
  if (!res.ok) return { points: null, error: data.error ?? `server said ${res.status}` } as T;
  return data;
}

async function fetchJson<T>(url: string): Promise<T> {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), 1500);
  try {
    const res = await fetch(url, { signal: ctrl.signal });
    if (!res.ok) throw new Error(String(res.status));
    return (await res.json()) as T;
  } finally {
    clearTimeout(t);
  }
}

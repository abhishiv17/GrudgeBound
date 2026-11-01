/**
 * Where the client gets the living world from:
 *  - "server": the game server (npm run server), polled every few seconds;
 *  - "offline": if no server is running, the same shared engine runs in the browser,
 *    sped up so you can watch the town change.
 * Call stop() when the scene is destroyed so no timers are left running.
 */
import { createWorld, createRng, step, summarize, type GameEvent, type WorldSummary } from "@grudgebound/shared";

const SERVER = process.env.NEXT_PUBLIC_SERVER_URL ?? "http://localhost:8787";
const POLL_MS = 3000;
const OFFLINE_TICK_MS = 2000;

export interface WorldFeed {
  mode: "server" | "offline";
  latest: WorldSummary;
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
    const feed: WorldFeed = { mode: "server", latest: first, stop: () => clearInterval(timer) };
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
    const feed: WorldFeed = { mode: "offline", latest: summarize(world), stop: () => clearInterval(timer) };
    onUpdate(feed.latest);
    return feed;
  }
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

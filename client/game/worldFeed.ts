/**
 * Where the client gets the living world from:
 *  - "server": the game server (npm run server), polled every few seconds;
 *  - "offline": if no server is running, the same shared engine runs in the browser,
 *    sped up so you can watch the town change.
 * Call stop() when the scene is destroyed so no timers are left running.
 *
 * With a server, battles run on it (startBattle / battleTurn, see battleClient.ts): it picks the curse and keeps
 * the seed secret, so results can't be faked. Offline, the battle runs in the browser and report() records it
 * in the offline town.
 */
import {
  createWorld, createRng, recordCapture, step, summarize,
  type CaptureReport, type GameEvent, type Omen, type PlayerAction, type WorldSummary,
} from "@grudgebound/shared";
import { postReply, type RemoteStart, type RemoteTurn, type Reply } from "./battleClient";

const SERVER = process.env.NEXT_PUBLIC_SERVER_URL ?? "http://localhost:8787";
/** NEAR account that gets this player's talismans and points (e.g. akshay.grudgebound.testnet); unset: the server's guest. */
const ACCOUNT = process.env.NEXT_PUBLIC_PLAYER_ACCOUNT?.trim().toLowerCase() || undefined;
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
  /** Offline only: record a battle fought in the browser. The server runs its own battles. */
  report(capture: CaptureReport, proof: BattleProof): Promise<ReportAnswer>;
  /** Server only (null offline): start a battle the server runs. */
  startBattle(ward: string, playerCurse: string, by: string): Promise<Reply<RemoteStart>> | null;
  battleTurn(battleId: string, action: PlayerAction): Promise<Reply<RemoteTurn>>;
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
      async report() {
        return { points: null, error: "the server runs its own battles" };
      },
      startBattle: (ward, playerCurse, by) =>
        postReply<RemoteStart>(`${SERVER}/battle/start`, { ward, playerCurse, by, ...(ACCOUNT ? { account: ACCOUNT } : {}) }),
      battleTurn: (battleId, action) => postReply<RemoteTurn>(`${SERVER}/battle/turn`, { battleId, action }),
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
      startBattle: () => null,
      battleTurn: async () => ({ ok: false, status: 0, error: "the offline town runs no server battles" }),
      stop: () => clearInterval(timer),
    };
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

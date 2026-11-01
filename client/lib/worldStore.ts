/**
 * One shared world feed for the whole client.
 *
 * The Phaser scene and every React panel read the same snapshot, so the town only runs once
 * (one server poll loop, or one offline engine in the browser). React reads it with
 * useSyncExternalStore(subscribe, getSnapshot, getServerSnapshot); Phaser calls subscribe directly.
 *
 * The feed starts with the first subscriber and stops a moment after the last one leaves.
 * The delay keeps React Strict Mode's mount/unmount/mount (and the title -> game switch)
 * from restarting the offline town from day 1.
 */
import type { CaptureReport, GameEvent, WorldSummary } from "@grudgebound/shared";
import { startWorldFeed, type BattleProof, type ReportAnswer, type WorldFeed } from "../game/worldFeed";

export type FeedStatus = "connecting" | "server" | "offline";

export interface Announcement {
  id: number;
  kind: string;
  text: string;
}

export interface WorldSnapshot {
  status: FeedStatus;
  summary: WorldSummary | null;
  /** Kessa's announcements, oldest first (capped). */
  announcements: Announcement[];
}

/** Event types Kessa reads out. */
const ANNOUNCED = new Set(["rule-added", "rule-rejected", "bankruptcy", "calamity", "reopen"]);
const MAX_ANNOUNCEMENTS = 30;
const STOP_DELAY_MS = 1500;

const INITIAL: WorldSnapshot = { status: "connecting", summary: null, announcements: [] };

let snapshot: WorldSnapshot = INITIAL;
let feed: WorldFeed | undefined;
let starting = false;
let stopTimer: ReturnType<typeof setTimeout> | undefined;
let nextAnnouncementId = 1;
const listeners = new Set<() => void>();

function emit(next: WorldSnapshot) {
  snapshot = next;
  for (const l of listeners) l();
}

function announce(kind: string, text: string) {
  const list = [...snapshot.announcements, { id: nextAnnouncementId++, kind, text }];
  emit({ ...snapshot, announcements: list.slice(-MAX_ANNOUNCEMENTS) });
}

function onEvent(e: GameEvent) {
  if (ANNOUNCED.has(e.type)) announce(e.type, e.text);
}

function start() {
  if (feed || starting) return;
  starting = true;
  void startWorldFeed(
    (s) => emit({ ...snapshot, summary: s }),
    onEvent,
  ).then((f) => {
    starting = false;
    if (listeners.size === 0) {
      f.stop();
      return;
    }
    feed = f;
    emit({ ...snapshot, status: f.mode, summary: f.latest });
    announce(
      "welcome",
      f.mode === "server"
        ? "Welcome to the Rite. The town is listening."
        : "Welcome to the Rite. No town server answered, so Hollowmere runs here in your browser.",
    );
  });
}

function stop() {
  feed?.stop();
  feed = undefined;
  snapshot = INITIAL;
}

export const worldStore = {
  subscribe(listener: () => void): () => void {
    listeners.add(listener);
    if (stopTimer) {
      clearTimeout(stopTimer);
      stopTimer = undefined;
    }
    start();
    return () => {
      listeners.delete(listener);
      if (listeners.size === 0 && !stopTimer) {
        stopTimer = setTimeout(() => {
          stopTimer = undefined;
          if (listeners.size === 0) stop();
        }, STOP_DELAY_MS);
      }
    };
  },
  getSnapshot(): WorldSnapshot {
    return snapshot;
  },
  getServerSnapshot(): WorldSnapshot {
    return INITIAL;
  },
  /** Kessa reads a line out in the banner (e.g. the result of your battle). */
  say(kind: string, text: string) {
    announce(kind, text);
  },
  /** Report a binding or exorcism to the town (server or offline engine). */
  async reportCapture(capture: CaptureReport, proof: BattleProof): Promise<ReportAnswer> {
    if (!feed) return { points: null, error: "the town is not running" };
    return feed.report(capture, proof);
  },
};

"use client";

import { useEffect, useState, useSyncExternalStore } from "react";
import { uiStore, type UiState } from "./uiStore";
import { worldStore, type WorldSnapshot } from "./worldStore";
import { playerStore, type PlayerState } from "./playerStore";

/** The shared world snapshot (starts the feed on first use). */
export function useWorld(): WorldSnapshot {
  return useSyncExternalStore(worldStore.subscribe, worldStore.getSnapshot, worldStore.getServerSnapshot);
}

export function useUi(): UiState {
  return useSyncExternalStore(uiStore.subscribe, uiStore.getSnapshot, uiStore.getSnapshot);
}

/** The player's own points, Ember and talismans. */
export function usePlayer(): PlayerState {
  return useSyncExternalStore(playerStore.subscribe, playerStore.getSnapshot, playerStore.getServerSnapshot);
}

export function useReducedMotion(): boolean {
  const [reduced, setReduced] = useState(false);
  useEffect(() => {
    const mq = window.matchMedia("(prefers-reduced-motion: reduce)");
    setReduced(mq.matches);
    const on = () => setReduced(mq.matches);
    mq.addEventListener("change", on);
    return () => mq.removeEventListener("change", on);
  }, []);
  return reduced;
}

/** Stand-in id for the signed-in player until email login lands (used as the author of local rule drafts). */
export const PLAYER_ID = "player-1";

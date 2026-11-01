"use client";

import { useEffect, useState, useSyncExternalStore } from "react";
import { uiStore, type UiState } from "./uiStore";
import { worldStore, type WorldSnapshot } from "./worldStore";

/** The shared world snapshot (starts the feed on first use). */
export function useWorld(): WorldSnapshot {
  return useSyncExternalStore(worldStore.subscribe, worldStore.getSnapshot, worldStore.getServerSnapshot);
}

export function useUi(): UiState {
  return useSyncExternalStore(uiStore.subscribe, uiStore.getSnapshot, uiStore.getSnapshot);
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

/** Stand-in for the signed-in player until email login lands: the engine's "Player 1". */
export const PLAYER_ID = "player-1";

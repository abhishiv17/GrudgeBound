"use client";

import { useState } from "react";
import TitleScreen from "./TitleScreen";
import GameShell from "./GameShell";
import { uiStore } from "../lib/uiStore";
import { playerStore } from "../lib/playerStore";

/** Title screen first; the Phaser game mounts only once the player enters the Ward. */
export default function GameApp() {
  const [screen, setScreen] = useState<"title" | "ward">("title");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  async function enter() {
    if (busy) return;
    setBusy(true); setError("");
    try {
      const response = await fetch("/api/game/auth/session", { cache: "no-store" });
      if (response.status === 401) { window.location.assign("/sign-in"); return; }
      if (response.status === 502) { playerStore.selectProfile(); setScreen("ward"); return; } // Existing offline fallback.
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || "Couldn't enter Hollowmere.");
      playerStore.selectProfile(result.player.id); setScreen("ward");
    } catch (e) { setError(e instanceof Error ? e.message : "Couldn't enter Hollowmere."); }
    finally { setBusy(false); }
  }
  if (screen === "title") return <><TitleScreen onStart={() => { void enter(); }} />{error && <p role="alert" style={{ position: "fixed", bottom: 32, left: 24, right: 24, zIndex: 100, color: "#fff0d1" }}>{error}</p>}</>;
  return (
    <GameShell
      onExit={() => {
        uiStore.closePanel();
        uiStore.closeEncounter();
        setScreen("title");
      }}
    />
  );
}

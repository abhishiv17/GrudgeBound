"use client";

import { useState } from "react";
import TitleScreen from "./TitleScreen";
import GameShell from "./GameShell";
import { uiStore } from "../lib/uiStore";

/** Title screen first; the Phaser game mounts only once the player enters the Ward. */
export default function GameApp() {
  const [screen, setScreen] = useState<"title" | "ward">("title");
  if (screen === "title") return <TitleScreen onStart={() => setScreen("ward")} />;
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

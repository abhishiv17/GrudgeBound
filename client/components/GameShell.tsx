"use client";

import { useEffect } from "react";
import GameCanvas from "./GameCanvas";
import WardStatus from "./hud/WardStatus";
import HudActions from "./hud/HudActions";
import KessaBanner from "./hud/KessaBanner";
import EncounterDialog from "./hud/EncounterDialog";
import TouchPad from "./hud/TouchPad";
import RulesPanel from "./panels/RulesPanel";
import VowShrine from "./panels/VowShrine";
import TalismanCase from "./panels/TalismanCase";
import { isBlocking, uiStore } from "../lib/uiStore";
import { useUi, useWorld } from "../lib/hooks";

/** The framed game: canvas, React HUD layered on top (stacked below it on phones), and the panels. */
export default function GameShell({ onExit }: { onExit: () => void }) {
  const { status } = useWorld();
  const ui = useUi();

  // Panel shortcuts: R, V and T (ignored while typing or while something else is open).
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey || e.altKey || isBlocking()) return;
      const t = e.target as HTMLElement | null;
      if (t?.closest("input, select, textarea")) return;
      const k = e.key.toLowerCase();
      if (k === "r") uiStore.openPanel("rules");
      if (k === "v") uiStore.openPanel("vows");
      if (k === "t") uiStore.openPanel("talismans");
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  return (
    <div className="shell">
      <header className="shell__bar">
        <button type="button" className="brand" onClick={onExit} title="Back to the title screen">
          <span className="brand__main">Grudgebound</span>
          <span className="brand__sub">The Rite</span>
        </button>
        <span className={`feed-pill feed-pill--${status}`} role="status">
          <span className="feed-pill__dot" aria-hidden />
          {status === "server" ? (
            <span>Connected<span className="feed-pill__more"> to Hollowmere</span></span>
          ) : status === "offline" ? (
            <span>Offline town<span className="feed-pill__more"> (runs in your browser)</span></span>
          ) : (
            "Lighting lanterns…"
          )}
        </span>
      </header>

      <div className={`stage ${ui.battle ? "stage--battle" : ""}`}>
        <div className="stage__frame">
          <GameCanvas />
        </div>
        <WardStatus className="stage__status" />
        <HudActions className="stage__actions" />
        <div className="stage__bottom">
          {ui.nearBoard && !isBlocking(ui) ? (
            <p className="prompt-chip">
              <kbd>E</kbd> Read the Rite board
            </p>
          ) : null}
          <KessaBanner />
          <EncounterDialog />
        </div>
        <TouchPad className="stage__pad" />
      </div>

      <footer className="shell__hints">
        <span><kbd>WASD</kbd>/<kbd>Arrows</kbd> walk</span>
        <span>Tall grass hides curses</span>
        <span><kbd>R</kbd> rules</span>
        <span><kbd>V</kbd> vows</span>
        <span><kbd>T</kbd> talismans</span>
        <span><kbd>Esc</kbd> close</span>
      </footer>

      <RulesPanel />
      <VowShrine />
      <TalismanCase />
    </div>
  );
}

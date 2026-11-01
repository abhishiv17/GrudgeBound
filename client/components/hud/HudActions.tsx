"use client";

import { CoinIcon, NoticeIcon, SealIcon } from "../ui/Icons";
import { uiStore } from "../../lib/uiStore";

/** The panel buttons on the HUD. Shortcuts: R, V and T. */
export default function HudActions({ className }: { className?: string }) {
  return (
    <nav className={`hud-actions ${className ?? ""}`} aria-label="Rite panels">
      <button type="button" className="btn btn--paper hud-actions__btn" onClick={() => uiStore.openPanel("rules")} aria-keyshortcuts="R">
        <NoticeIcon scale={2} />
        <span>Rewrite the Rules</span>
        <kbd aria-hidden>R</kbd>
      </button>
      <button type="button" className="btn btn--paper hud-actions__btn" onClick={() => uiStore.openPanel("vows")} aria-keyshortcuts="V">
        <SealIcon scale={2} />
        <span>Vow Shrine</span>
        <kbd aria-hidden>V</kbd>
      </button>
      <button type="button" className="btn btn--paper hud-actions__btn" onClick={() => uiStore.openPanel("talismans")} aria-keyshortcuts="T">
        <CoinIcon scale={2} />
        <span>Talismans</span>
        <kbd aria-hidden>T</kbd>
      </button>
    </nav>
  );
}

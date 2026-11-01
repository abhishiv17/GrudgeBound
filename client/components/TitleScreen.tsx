"use client";

import { useMemo, useState } from "react";
import Kessa from "./Kessa";
import Panel from "./ui/Panel";
import PixelSprite from "./ui/PixelSprite";
import { CoinIcon, NoticeIcon } from "./ui/Icons";
import { playerFrame } from "../game/playerArt";
import { useWorld } from "../lib/hooks";

/** Lanterns strung across the top of the title screen (x positions in %, sag handled by CSS). */
const STRING_LANTERNS = [6, 15, 24, 33, 42, 58, 67, 76, 85, 94];

export default function TitleScreen({ onStart }: { onStart: () => void }) {
  const [help, setHelp] = useState(false);
  const world = useWorld();
  const player = useMemo(() => playerFrame("side", 0), []);
  const s = world.summary;

  return (
    <main className="title-screen">
      <div className="title-sky" aria-hidden>
        <span className="title-sky__moon" />
      </div>

      <div className="title-strings" aria-hidden>
        <svg className="title-strings__line" viewBox="0 0 100 10" preserveAspectRatio="none">
          <path d="M0 1 Q 25 9 50 2 Q 75 9 100 1" fill="none" stroke="#3a2a20" strokeWidth="0.35" vectorEffect="non-scaling-stroke" />
        </svg>
        {STRING_LANTERNS.map((x, i) => {
          // follow the two sagging arcs of the path above
          const t = x < 50 ? x / 50 : (x - 50) / 50;
          const y = (x < 50 ? 1 : 2) * (1 - t) * (1 - t) + 2 * (1 - t) * t * 9 + t * t * (x < 50 ? 2 : 1);
          return (
            <span
              key={x}
              className={`title-strings__item ${i % 2 ? "title-strings__item--flag" : "title-strings__item--lantern"}`}
              style={{ left: `${x}%`, top: `${y * 10}%`, animationDelay: `${i * -0.37}s` }}
            />
          );
        })}
      </div>

      <div className="title-town" aria-hidden>
        <div className="title-town__far" />
        <div className="title-town__spire"><span /></div>
        <div className="title-town__near" />
        <div className="title-town__water" />
      </div>

      <div className="title-scraps" aria-hidden>
        {Array.from({ length: 9 }, (_, i) => (
          <span key={i} style={{ left: `${(i * 37) % 100}%`, animationDelay: `${i * -2.3}s`, animationDuration: `${14 + (i % 4) * 3}s` }} />
        ))}
      </div>

      <div className="title-content">
        <h1 className="logo">
          <span className="logo__main">Grudgebound</span>
          <span className="logo__sub">The Rite</span>
        </h1>
        <p className="tagline">Greed makes monsters. You rewrite the rules.</p>

        <Panel className="title-menu" as="nav">
          <h2 className="sr-only">Start</h2>
          <button type="button" className="btn btn--primary btn--big" onClick={onStart} autoFocus>
            Enter Lantern Ward
          </button>
          <button type="button" className="btn btn--paper" aria-expanded={help} aria-controls="how-to-play" onClick={() => setHelp((h) => !h)}>
            How to play
          </button>
          <div id="how-to-play" className="title-help" hidden={!help}>
            <ul>
              <li><kbd>WASD</kbd> / <kbd>Arrows</kbd> walk the night market.</li>
              <li>Tall grass hides curses born from the Ward&apos;s Grudge.</li>
              <li><kbd>R</kbd> Rewrite the Rules &middot; <kbd>V</kbd> Vow Shrine &middot; <kbd>E</kbd> at the Rite board.</li>
              <li><kbd>Esc</kbd> closes any panel.</li>
            </ul>
          </div>
          <p className="title-status" aria-live="polite">
            {s ? (
              <>
                <span>Day {s.day}</span>
                <span className="title-status__dot" aria-hidden>◆</span>
                <span><NoticeIcon scale={1} /> {s.rules.length} rules in force</span>
                <span className="title-status__dot" aria-hidden>◆</span>
                <span><CoinIcon scale={1} /> {s.burnedEmber} Ember burned</span>
              </>
            ) : (
              <span>Kessa is lighting the lanterns…</span>
            )}
          </p>
        </Panel>
      </div>

      <div className="title-foreground" aria-hidden>
        <div className="title-ledge" />
        <PixelSprite grid={player} scale={4} className="title-player" />
        <Kessa scale={5} className="title-kessa" />
      </div>

      <footer className="title-footer">
        <span>Hollowmere, a harbor town that ran on debt</span>
        <span>NEAR HACKATHON 2026</span>
      </footer>
    </main>
  );
}

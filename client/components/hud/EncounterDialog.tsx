"use client";

import { useEffect, useId, useRef } from "react";
import { EMOTION_INFO, WARD_INFO, type CurseSpecies } from "@grudgebound/shared";
import Panel from "../ui/Panel";
import PixelSprite from "../ui/PixelSprite";
import Stamp from "../ui/Stamp";
import { EmotionIcon } from "../ui/Icons";
import { CURSE_PORTRAIT } from "../../game/pixelArt";
import { uiStore } from "../../lib/uiStore";
import { useUi } from "../../lib/hooks";

const OMEN_SCALE = { whisper: 4, wail: 5, calamity: 6 } as const;
const STAT_MAX = { hp: 160, attack: 24, guard: 22, speed: 13 } as const;
const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);

/** "A wild curse appears!" on a parchment panel. Space, Enter or Esc continues. */
export default function EncounterDialog({ className }: { className?: string }) {
  const { encounter } = useUi();
  return encounter ? <Encounter curse={encounter} className={className} /> : null;
}

function Encounter({ curse, className }: { curse: CurseSpecies; className?: string }) {
  const titleId = useId();
  const button = useRef<HTMLButtonElement>(null);
  const info = EMOTION_INFO[curse.emotion];

  useEffect(() => {
    button.current?.focus();
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        e.preventDefault();
        uiStore.closeEncounter();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  return (
    <div className={`encounter ${className ?? ""}`} role="alertdialog" aria-modal="false" aria-labelledby={titleId} aria-describedby={`${titleId}-d`}>
      <Panel className={`encounter__panel encounter__panel--${curse.emotion}`}>
        <div className="encounter__portrait" style={{ ["--emotion" as string]: info.color }}>
          <PixelSprite map={CURSE_PORTRAIT[curse.emotion]} scale={OMEN_SCALE[curse.omen]} title={`${curse.name}, a ${info.label} curse`} />
          {curse.omen === "calamity" ? <Stamp className="encounter__stamp">Calamity</Stamp> : null}
        </div>
        <div className="encounter__body">
          <p className="encounter__kicker">Something stirs in the tall grass</p>
          <h2 className="encounter__title" id={titleId}>
            A wild <span style={{ color: info.color }}>{curse.name}</span> appears!
          </h2>
          <ul className="encounter__tags">
            <li className={`tag tag--${curse.emotion}`}><EmotionIcon emotion={curse.emotion} scale={1} /> {info.label}</li>
            <li className={`tag tag--omen-${curse.omen}`}>{cap(curse.omen)}</li>
            <li className="tag">{WARD_INFO[curse.home].name}</li>
          </ul>
          <div id={`${titleId}-d`}>
            <p className="encounter__line">Born from <em>{curse.bornFrom}</em>.</p>
            <p className="encounter__line">
              Signature move: <b>{curse.signatureMove.name}</b>, {curse.signatureMove.effect}.
            </p>
          </div>
          <dl className="encounter__stats">
            {(Object.keys(STAT_MAX) as (keyof typeof STAT_MAX)[]).map((k) => (
              <div key={k}>
                <dt>{k === "hp" ? "HP" : cap(k)}</dt>
                <dd>
                  <span className="mini-meter" aria-hidden>
                    <span style={{ width: `${Math.min(100, (curse.stats[k] / STAT_MAX[k]) * 100)}%` }} />
                  </span>
                  {curse.stats[k]}
                </dd>
              </div>
            ))}
          </dl>
          <div className="encounter__foot">
            <p className="encounter__note">Battles and binding arrive on Days 3-6.</p>
            <button ref={button} type="button" className="btn btn--primary" onClick={() => uiStore.closeEncounter()}>
              Continue <kbd aria-hidden>Space</kbd>
            </button>
          </div>
        </div>
      </Panel>
    </div>
  );
}

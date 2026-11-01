"use client";

import { GRUDGE_SOURCES, OATHBREAKER_DAYS, VOW_KEEP_BONUS, VOW_TEMPLATES, vowPayout } from "@grudgebound/shared";
import Modal from "../ui/Modal";
import PixelSprite from "../ui/PixelSprite";
import { CoinIcon, EmotionIcon } from "../ui/Icons";
import { SEAL } from "../../game/pixelArt";
import { uiStore } from "../../lib/uiStore";
import { useUi } from "../../lib/hooks";

function VowText({ text }: { text: string }) {
  return (
    <>
      {text.split(/(\{\w+\})/g).map((part, i) =>
        part.startsWith("{") ? <span key={i} className="blank">{part.slice(1, -1)}</span> : <span key={i}>{part}</span>,
      )}
    </>
  );
}

const days = ([min, max]: [number, number]) => (min === max ? `${min} day${min === 1 ? "" : "s"}` : `${min}-${max} days`);

/** The Vow Shrine: every vow you can swear, what it costs and what it grants. */
export default function VowShrine() {
  const { panel } = useUi();
  return (
    <Modal
      open={panel === "vows"}
      onClose={uiStore.closePanel}
      title="Vow Shrine"
      subtitle="Accept a limit, gain a power. Stake points on keeping your word."
      className="modal--wide"
    >
      <div className="vows">
        <ul className="vow-list">
          {VOW_TEMPLATES.map((v) => (
            <li key={v.id} className="vow">
              <div className="vow__scroll">
                <span className="vow__id">{v.id}</span>
                <p className="vow__oath">
                  &ldquo;<VowText text={v.text} />&rdquo;
                </p>
                <p className="vow__power">
                  <span className="vow__label">Power</span> {v.power}
                </p>
                <dl className="vow__terms">
                  <div>
                    <dt>Lasts</dt>
                    <dd>{days(v.durationDays)}</dd>
                  </div>
                  <div>
                    <dt>Stake</dt>
                    <dd><CoinIcon scale={1} /> {v.stake} pts</dd>
                  </div>
                  <div>
                    <dt>If kept</dt>
                    <dd className="vow__kept">+{vowPayout(v.stake, true)} pts back</dd>
                  </div>
                </dl>
              </div>
              <PixelSprite map={SEAL} scale={3} className="vow__seal" />
            </li>
          ))}
        </ul>
        <aside className="vows__law">
          <h3 className="section-title">The shrine&apos;s law</h3>
          <ul>
            <li><b>Kept:</b> your stake comes back with {Math.round(VOW_KEEP_BONUS * 100)}% more.</li>
            <li>
              <b>Broken:</b> the stake burns, you are an Oathbreaker for {OATHBREAKER_DAYS} day{OATHBREAKER_DAYS === 1 ? "" : "s"} (worse prices),
              and the Ward gains <EmotionIcon emotion="dread" scale={1} /> {GRUDGE_SOURCES.vowBroken.amount} Dread.
            </li>
            <li>The Rite contract holds every stake, so nobody can quietly undo a vow.</li>
          </ul>
          <p className="rules__fineprint">Swearing vows opens when the Rite contract goes live. For now, read the terms.</p>
        </aside>
      </div>
    </Modal>
  );
}

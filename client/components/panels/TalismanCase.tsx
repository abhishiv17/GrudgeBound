"use client";

import { EMOTION_INFO, curseById } from "@grudgebound/shared";
import Modal from "../ui/Modal";
import PixelSprite from "../ui/PixelSprite";
import { CoinIcon, EmotionIcon } from "../ui/Icons";
import { CURSE_PORTRAIT } from "../../game/pixelArt";
import { uiStore } from "../../lib/uiStore";
import { playerStore, type BattleEntry, type Talisman } from "../../lib/playerStore";
import { usePlayer, useUi } from "../../lib/hooks";

const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);
const VERB: Record<BattleEntry["outcome"], string> = { bound: "Bound", won: "Exorcised", fled: "Slipped away from", lost: "Driven off by" };

/** Your talisman case: every curse you have bound, your partner, points, Ember and recent fights. Shortcut: T. */
export default function TalismanCase() {
  const { panel } = useUi();
  const player = usePlayer();
  const { record } = player;
  return (
    <Modal
      open={panel === "talismans"}
      onClose={uiStore.closePanel}
      title="Talisman Case"
      subtitle="Every curse you bind becomes a talisman you own. Your partner fights for you."
      className="modal--wide"
    >
      <div className="case">
        <dl className="case__purse">
          <div>
            <dt>Points</dt>
            <dd><CoinIcon scale={1} /> {player.points}</dd>
          </div>
          <div>
            <dt>Ember</dt>
            <dd>{player.ember}</dd>
          </div>
          <div>
            <dt>Talismans</dt>
            <dd>{player.talismans.length}</dd>
          </div>
          <div>
            <dt>Record</dt>
            <dd className="case__record">
              {record.bound} bound · {record.won} exorcised · {record.fled} fled · {record.lost} lost
            </dd>
          </div>
        </dl>

        <ul className="case__grid">
          {player.talismans.map((t) => (
            <TalismanCard key={t.id} t={t} partner={t.id === player.partnerId} />
          ))}
        </ul>

        <section className="case__history" aria-label="Recent battles">
          <h3 className="section-title">Recent battles</h3>
          {player.history.length ? (
            <ol>
              {player.history.slice(0, 8).map((h, i) => (
                <li key={`${h.seed}-${i}`}>
                  <span className="case__day">Day {h.day}</span>
                  {VERB[h.outcome]} <b>{curseById(h.wild).name}</b> with {curseById(h.partner).name}
                  {h.points ? <span className="case__pts"> +{h.points}</span> : null}
                </li>
              ))}
            </ol>
          ) : (
            <p className="rules__fineprint">No fights yet. Walk through the tall grass in Lantern Ward.</p>
          )}
        </section>
      </div>
    </Modal>
  );
}

function TalismanCard({ t, partner }: { t: Talisman; partner: boolean }) {
  const sp = curseById(t.speciesId);
  const info = EMOTION_INFO[sp.emotion];
  return (
    <li className={`talisman ${partner ? "is-partner" : ""}`}>
      <div className="talisman__portrait" style={{ ["--emotion" as string]: info.color }}>
        <PixelSprite map={CURSE_PORTRAIT[sp.emotion]} scale={3} title={`${sp.name}, a ${info.label} curse`} />
      </div>
      <div className="talisman__body">
        <h3 className="talisman__name">{sp.name}</h3>
        <p className="talisman__tags">
          <span className={`tag tag--${sp.emotion}`}><EmotionIcon emotion={sp.emotion} scale={1} /> {info.label}</span>
          <span className={`tag tag--omen-${t.omen}`}>{cap(t.omen)}</span>
          {partner ? <span className="tag talisman__partner">Partner</span> : null}
        </p>
        <p className="talisman__line">{t.boundDay === null ? "A gift from Kessa" : `Bound on Day ${t.boundDay}`} · {t.wins} win{t.wins === 1 ? "" : "s"}</p>
        <p className="talisman__line">
          HP {sp.stats.hp} · Atk {sp.stats.attack} · Grd {sp.stats.guard} · Spd {sp.stats.speed}
        </p>
        <button
          type="button"
          className="btn btn--paper talisman__btn"
          onClick={() => playerStore.setPartner(t.id)}
          disabled={partner}
        >
          {partner ? "Fighting for you" : "Make partner"}
        </button>
      </div>
    </li>
  );
}

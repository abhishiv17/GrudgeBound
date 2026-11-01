import { EMOTIONS, EMOTION_INFO, GRUDGE_MAX, type GrudgeMix } from "@grudgebound/shared";
import { EmotionIcon } from "../ui/Icons";

/** A pixel ink-meter of a Ward's Grudge, split into Rage, Sorrow and Dread. */
export default function GrudgeMeter({ grudge, total }: { grudge: GrudgeMix | null; total: number }) {
  const g = grudge ?? { rage: 0, sorrow: 0, dread: 0 };
  const text = EMOTIONS.map((e) => `${EMOTION_INFO[e].label} ${Math.round(g[e])}`).join(", ");
  return (
    <div className="grudge">
      <div className="grudge__head">
        <span className="hud-label">Grudge</span>
        <span className="grudge__total">
          <b>{Math.round(total)}</b>/{GRUDGE_MAX}
        </span>
      </div>
      <div
        className="meter"
        role="meter"
        aria-label="Grudge"
        aria-valuemin={0}
        aria-valuemax={GRUDGE_MAX}
        aria-valuenow={Math.round(total)}
        aria-valuetext={`${Math.round(total)} of ${GRUDGE_MAX}: ${text}`}
      >
        {EMOTIONS.map((e) => (
          <span key={e} className={`meter__fill meter__fill--${e}`} style={{ width: `${Math.min(100, (g[e] / GRUDGE_MAX) * 100)}%` }} />
        ))}
      </div>
      <ul className="grudge__legend" aria-hidden>
        {EMOTIONS.map((e) => (
          <li key={e} className={`grudge__chip grudge__chip--${e}`}>
            <EmotionIcon emotion={e} scale={2} />
            <span>{EMOTION_INFO[e].label}</span>
            <b>{Math.round(g[e])}</b>
          </li>
        ))}
      </ul>
    </div>
  );
}

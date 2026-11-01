"use client";

import { MAX_ACTIVE_RULES, WARD_INFO } from "@grudgebound/shared";
import GrudgeMeter from "./GrudgeMeter";
import { CoinIcon, LanternIcon, MoonIcon, NoticeIcon, SunIcon } from "../ui/Icons";
import { usePlayer, useUi, useWorld } from "../../lib/hooks";

/** Ward card: name, day and hour (with a night marker), Grudge, rules in force, and the player's points and Ember. */
export default function WardStatus({ className }: { className?: string }) {
  const { summary } = useWorld();
  const { sealed } = useUi();
  const ward = summary?.wards.find((w) => w.id === "lantern");
  const me = usePlayer();
  const night = summary?.night ?? true;
  const hour = summary ? `${String(summary.hour).padStart(2, "0")}:00` : "--:--";

  return (
    <section className={`hud-card ${className ?? ""}`} aria-label="Ward status">
      <header className="hud-card__head">
        <LanternIcon scale={2} />
        <h2 className="hud-card__ward">{WARD_INFO.lantern.name}</h2>
        <span className={`time-badge ${night ? "time-badge--night" : "time-badge--day"}`}>
          {night ? <MoonIcon scale={1} /> : <SunIcon scale={1} />}
          <span>
            Day {summary?.day ?? 1} · {hour}
          </span>
          <span className="sr-only">{night ? "(night)" : "(day)"}</span>
        </span>
      </header>
      <GrudgeMeter grudge={ward?.grudge ?? null} total={ward?.total ?? 0} />
      <dl className="hud-card__stats">
        <div>
          <dt><NoticeIcon scale={1} /> Rules in force</dt>
          <dd>
            {summary?.rules.length ?? 0}
            <small>/{MAX_ACTIVE_RULES}</small>
            {sealed.length ? <small className="hud-card__draft"> +{sealed.length} draft</small> : null}
          </dd>
        </div>
        <div>
          <dt><CoinIcon scale={1} /> Your points</dt>
          <dd>{me.points}</dd>
        </div>
        <div>
          <dt>Ember</dt>
          <dd>{me.ember}</dd>
        </div>
      </dl>
    </section>
  );
}

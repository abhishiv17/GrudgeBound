"use client";

import { useEffect, useId, useMemo, useState } from "react";
import {
  EMOTION_INFO, MAX_ACTIVE_RULES, RULE_BASE_COST, RULE_COST_STEP, RULE_TEMPLATES, WARD_INFO,
  arbiterCheck, renderRule, ruleCost, templateById, validateParams, wardsTouched,
  type ParamSpec, type Rule, type RuleParams, type WardId, type Emotion,
} from "@grudgebound/shared";
import Modal from "../ui/Modal";
import Kessa from "../Kessa";
import Stamp from "../ui/Stamp";
import PixelSprite from "../ui/PixelSprite";
import { CoinIcon } from "../ui/Icons";
import { SEAL } from "../../game/pixelArt";
import { uiStore, type LocalRule } from "../../lib/uiStore";
import { PLAYER_ID, usePlayer, useUi, useWorld } from "../../lib/hooks";

const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);

function optionLabel(key: string, value: string): string {
  if (key === "ward" && value in WARD_INFO) return WARD_INFO[value as WardId].name;
  if (key === "emotion" && value in EMOTION_INFO) return EMOTION_INFO[value as Emotion].label;
  if (key === "multiplier") return `x${value}`;
  return cap(value);
}

function defaults(templateId: string, keep: RuleParams = {}): RuleParams {
  const out: RuleParams = {};
  for (const [key, spec] of Object.entries(templateById(templateId).params)) {
    const prev = keep[key];
    if (spec.kind === "choice") out[key] = prev !== undefined && spec.options.includes(String(prev)) ? String(prev) : spec.options[0]!;
    else out[key] = typeof prev === "number" && prev >= spec.min && prev <= spec.max ? prev : spec.min;
  }
  return out;
}

/** Template text with its blanks shown as little underlined slots. */
function TemplateText({ text }: { text: string }) {
  return (
    <>
      {text.split(/(\{\w+\})/g).map((part, i) =>
        part.startsWith("{") ? <span key={i} className="blank">{part.slice(1, -1)}</span> : <span key={i}>{part}</span>,
      )}
    </>
  );
}

export default function RulesPanel() {
  const { panel } = useUi();
  return (
    <Modal
      open={panel === "rules"}
      onClose={uiStore.closePanel}
      title="Rewrite the Rules"
      subtitle="Pick a rule, fill in the blanks, and let Kessa judge it before you press the seal."
      className="modal--wide"
    >
      <RulesForm />
    </Modal>
  );
}

function RulesForm() {
  const { summary } = useWorld();
  const { sealed } = useUi();
  const [templateId, setTemplateId] = useState(RULE_TEMPLATES[0]!.id);
  const [params, setParams] = useState<RuleParams>(() => defaults(RULE_TEMPLATES[0]!.id));
  const [justSealed, setJustSealed] = useState<string | null>(null);
  const formId = useId();

  useEffect(() => {
    if (!justSealed) return;
    const t = setTimeout(() => setJustSealed(null), 4000);
    return () => clearTimeout(t);
  }, [justSealed]);

  const template = templateById(templateId);
  const active: Rule[] = useMemo(() => [
    ...(summary?.rules ?? []).map((r) => ({ id: r.id, templateId: r.templateId, params: r.params, author: r.author, cost: r.cost, addedAtTick: 0 })),
    ...sealed.map((r, i) => ({ id: -(i + 1), templateId: r.templateId, params: r.params, author: PLAYER_ID, cost: r.cost, addedAtTick: 0 })),
  ], [summary?.rules, sealed]);

  const errors = validateParams(templateId, params);
  const preview = renderRule(templateId, params);
  const cost = ruleCost(active.length);
  const verdict = arbiterCheck(templateId, params, active);
  const myPoints = usePlayer().points;
  const names = new Map((summary?.leaderboard ?? []).map((p) => [p.id, p.name]));

  const pick = (id: string) => {
    setTemplateId(id);
    setParams((p) => defaults(id, p));
    setJustSealed(null);
  };

  const seal = () => {
    if (!verdict.accepted) return;
    const rule: LocalRule = {
      id: `local-${Date.now()}`,
      templateId,
      params: { ...params },
      text: preview,
      cost,
      cruel: verdict.cruel,
    };
    uiStore.seal(rule);
    setJustSealed(preview);
  };

  let kessaLine: string;
  // local drafts carry negative ids; name them plainly instead of "rule #-1"
  if (!verdict.accepted) kessaLine = verdict.reason.replace(/rule #-\d+/g, "one of your sealed drafts");
  else if (verdict.cruel) {
    const touched = wardsTouched(params).map((w) => WARD_INFO[w].name).join(", ");
    kessaLine = `Allowed, but it is cruel. Dread will rise in ${touched}. Are you sure?`;
  } else kessaLine = "The Rite can hold this. Press the seal and I will read it to the town.";

  return (
    <div className="rules">
      <fieldset className="rules__templates">
        <legend className="section-title">1. Choose a rule</legend>
        <div className="template-list">
          {RULE_TEMPLATES.map((t) => {
            const cruel = typeof t.cruel === "function" ? "maybe" : t.cruel ? "yes" : "no";
            return (
              <label key={t.id} className={`template ${t.id === templateId ? "is-selected" : ""}`}>
                <input type="radio" name={`${formId}-tpl`} value={t.id} checked={t.id === templateId} onChange={() => pick(t.id)} />
                <span className="template__id">{t.id}</span>
                <span className="template__text"><TemplateText text={t.text} /></span>
                <span className="template__tags">
                  {cruel !== "no" ? <span className="tag tag--dread">{cruel === "maybe" ? "can be cruel" : "cruel"}</span> : null}
                  <span className="tag">{t.enforcedBy === "contract" ? "contract" : "server"}</span>
                </span>
              </label>
            );
          })}
        </div>
      </fieldset>

      <div className="rules__compose">
        <fieldset className="rules__params">
          <legend className="section-title">2. Fill the blanks</legend>
          {Object.entries(template.params).map(([key, spec]) => (
            <ParamField key={`${templateId}-${key}`} id={`${formId}-${key}`} name={key} spec={spec} value={params[key]} onChange={(v) => setParams((p) => ({ ...p, [key]: v }))} />
          ))}
          {errors.length ? <p className="field-error" role="alert">{errors.join("; ")}</p> : null}
        </fieldset>

        <div className="rules__preview">
          <h3 className="section-title">3. The notice</h3>
          <div className={`notice ${justSealed ? "is-sealed" : ""}`}>
            <span className="notice__pin" aria-hidden />
            <p className="notice__text" aria-live="polite">{preview}</p>
            <p className="notice__meta">
              <span>{template.enforcedBy === "contract" ? "Enforced by the Rite contract" : "Enforced by the game server"}</span>
              {verdict.accepted && verdict.cruel ? <span className="tag tag--dread">cruel</span> : null}
            </p>
            {justSealed ? <Stamp className="notice__stamp">Sealed</Stamp> : null}
          </div>

          <div className="cost" aria-live="polite">
            <CoinIcon scale={2} />
            <div>
              <p className="cost__line"><b>{cost}</b> points</p>
              <p className="cost__why">{RULE_BASE_COST} + {RULE_COST_STEP} for each of {active.length} rule{active.length === 1 ? "" : "s"} in force (max {MAX_ACTIVE_RULES})</p>
              <p className={`cost__purse ${myPoints < cost ? "is-short" : ""}`}>
                You hold {myPoints} point{myPoints === 1 ? "" : "s"}
                {myPoints < cost ? `: ${cost - myPoints} short once sealing goes on-chain` : ""}
              </p>
            </div>
          </div>

          <div className={`verdict ${verdict.accepted ? (verdict.cruel ? "verdict--cruel" : "verdict--ok") : "verdict--no"}`} aria-live="polite">
            <Kessa scale={3} strips={false} mood={verdict.accepted ? "happy" : "stern"} />
            <div className="verdict__bubble">
              <span className="verdict__who">
                Kessa&apos;s verdict: <b>{verdict.accepted ? (verdict.cruel ? "allowed, cruel" : "allowed") : "refused"}</b>
              </span>
              <p>{kessaLine}</p>
            </div>
          </div>

          <button type="button" className="btn btn--primary btn--seal" onClick={seal} disabled={!verdict.accepted}>
            <PixelSprite map={SEAL} scale={2} />
            Press the seal
          </button>
          <p className="rules__fineprint" role="status">
            {justSealed
              ? "Sealed on your desk. It stays local for now: sending rules to the Rite arrives with the chain."
              : "Sealing is local for now. Nothing is sent to the chain yet."}
          </p>
        </div>
      </div>

      <section className="rules__active" aria-labelledby={`${formId}-active`}>
        <h3 className="section-title" id={`${formId}-active`}>Rules in force</h3>
        {active.length === 0 ? (
          <p className="empty">No rules yet. The Rite is a blank board.</p>
        ) : (
          <ol className="rule-list">
            {(summary?.rules ?? []).map((r) => (
              <li key={r.id} className="rule-list__item">
                <span className="rule-list__no">#{r.id}</span>
                <span className="rule-list__text">{r.text}</span>
                <span className="rule-list__by">by {names.get(r.author) ?? r.author} · {r.cost} pts</span>
              </li>
            ))}
            {sealed.map((r) => (
              <li key={r.id} className="rule-list__item rule-list__item--draft">
                <span className="rule-list__no">draft</span>
                <span className="rule-list__text">{r.text}</span>
                <span className="rule-list__by">
                  yours · local only · {r.cost} pts
                  <button type="button" className="link-btn" onClick={() => uiStore.unseal(r.id)}>
                    tear up<span className="sr-only"> draft: {r.text}</span>
                  </button>
                </span>
              </li>
            ))}
          </ol>
        )}
      </section>
    </div>
  );
}

function ParamField({ id, name, spec, value, onChange }: { id: string; name: string; spec: ParamSpec; value: string | number | undefined; onChange: (v: string | number) => void }) {
  if (spec.kind === "choice") {
    return (
      <div className="field">
        <label htmlFor={id} className="field__label">{cap(name)}</label>
        <select id={id} className="field__select" value={String(value ?? "")} onChange={(e) => onChange(e.target.value)}>
          {spec.options.map((o) => (
            <option key={o} value={o}>{optionLabel(name, o)}</option>
          ))}
        </select>
      </div>
    );
  }
  const n = Number(value ?? spec.min);
  return (
    <div className="field">
      <label htmlFor={id} className="field__label">
        {cap(name)} <output htmlFor={id} className="field__value">{n}</output>
      </label>
      <input id={id} className="field__range" type="range" min={spec.min} max={spec.max} step={1} value={n} onChange={(e) => onChange(Number(e.target.value))} />
      <span className="field__bounds" aria-hidden><span>{spec.min}</span><span>{spec.max}</span></span>
    </div>
  );
}

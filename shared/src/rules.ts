/**
 * Rules: participants spend points to add a rule to the Rite, for everyone.
 * Rules are built from templates (no free text). The Arbiter validates each proposal.
 * Plan section 7.5 and Appendix B.
 */
import { WARDS, type WardId } from "./wards";
import { EMOTIONS } from "./emotions";

export const RULE_BASE_COST = 50;
export const RULE_COST_STEP = 25;
export const MAX_ACTIVE_RULES = 10;
export const REPEAL_MULTIPLIER = 2;

/** Cost to add a rule: 50 + 25 for every rule already in force. */
export function ruleCost(activeRuleCount: number): number {
  return RULE_BASE_COST + RULE_COST_STEP * activeRuleCount;
}

export const ACTIONS = ["exorcise", "bind", "bounty", "pvp", "trade"] as const;
export type Action = (typeof ACTIONS)[number];

export type ParamSpec =
  | { kind: "choice"; options: readonly string[] }
  | { kind: "number"; min: number; max: number };

export type RuleParams = Record<string, string | number>;

export interface RuleTemplate {
  id: string;
  text: string;
  params: Record<string, ParamSpec>;
  enforcedBy: "contract" | "server";
  /** Cruel rules add Dread to the Ward(s) they touch. */
  cruel: boolean | ((p: RuleParams) => boolean);
  /** Part of the MUST set of 8 for the hackathon build. */
  mvp: boolean;
}

const ward: ParamSpec = { kind: "choice", options: WARDS };
const action: ParamSpec = { kind: "choice", options: ACTIONS };

export const RULE_TEMPLATES: RuleTemplate[] = [
  { id: "R1", text: "{action} in {ward} gives x{multiplier} points", enforcedBy: "contract", cruel: false, mvp: true,
    params: { action, ward, multiplier: { kind: "choice", options: ["1.5", "2"] } } },
  { id: "R2", text: "{action} is forbidden in {ward} during the {time}", enforcedBy: "server", cruel: true, mvp: true,
    params: { action, ward, time: { kind: "choice", options: ["day", "night"] } } },
  { id: "R3", text: "Entering {ward} costs {amount} points, paid to {payee}", enforcedBy: "contract", cruel: true, mvp: true,
    params: { ward, amount: { kind: "number", min: 1, max: 10 }, payee: { kind: "choice", options: ["author", "guild"] } } },
  { id: "R4", text: "Fleeing a battle costs {amount} points", enforcedBy: "contract", cruel: false, mvp: true,
    params: { amount: { kind: "number", min: 1, max: 20 } } },
  { id: "R5", text: "Defeating {target} gives +{amount} points", enforcedBy: "contract", cruel: false, mvp: true,
    params: { target: { kind: "choice", options: ["the points leader", "the top 3"] }, amount: { kind: "number", min: 5, max: 30 } } },
  { id: "R6", text: "{emotion} curses cannot be used in {ward}", enforcedBy: "server", cruel: true, mvp: true,
    params: { emotion: { kind: "choice", options: EMOTIONS }, ward } },
  { id: "R7", text: "Every talisman trade pays {percent}% Ember to {payee}", enforcedBy: "contract", mvp: true,
    cruel: (p) => p.payee === "author",
    params: { percent: { kind: "number", min: 1, max: 10 }, payee: { kind: "choice", options: ["author", "guild", "burn"] } } },
  { id: "R8", text: "No PvP in {ward}", enforcedBy: "server", cruel: false, mvp: true, params: { ward } },
  { id: "R9", text: "While {ward} Grudge is above {threshold}, everyone there loses {amount} points per day", enforcedBy: "contract", cruel: true, mvp: false,
    params: { ward, threshold: { kind: "number", min: 40, max: 90 }, amount: { kind: "number", min: 1, max: 5 } } },
  { id: "R10", text: "Players with an active vow earn +{percent}% points", enforcedBy: "contract", cruel: false, mvp: false,
    params: { percent: { kind: "number", min: 10, max: 50 } } },
  { id: "R11", text: "Calamity curses may only rise in {ward}", enforcedBy: "server", cruel: false, mvp: false, params: { ward } },
  { id: "R12", text: "Each participant may change Ward only once per {days} days", enforcedBy: "server", cruel: true, mvp: false,
    params: { days: { kind: "number", min: 1, max: 3 } } },
];

export interface Rule {
  id: number;
  templateId: string;
  params: RuleParams;
  author: string;
  cost: number;
  addedAtTick: number;
}

export function templateById(id: string): RuleTemplate {
  const t = RULE_TEMPLATES.find((x) => x.id === id);
  if (!t) throw new Error(`Unknown rule template: ${id}`);
  return t;
}

export function renderRule(templateId: string, params: RuleParams): string {
  const t = templateById(templateId);
  return t.text.replace(/\{(\w+)\}/g, (_, k: string) => {
    const v = params[k];
    if (v === undefined) return `{${k}}`;
    if (k === "ward") return wardName(String(v));
    if (k === "action" || k === "emotion") {
      const s = String(v);
      return s === "pvp" ? "PvP" : s.charAt(0).toUpperCase() + s.slice(1);
    }
    return String(v);
  }).replace(/^./, (c) => c.toUpperCase());
}

function wardName(id: string): string {
  return id.charAt(0).toUpperCase() + id.slice(1) + " Ward";
}

export function isCruel(templateId: string, params: RuleParams): boolean {
  const c = templateById(templateId).cruel;
  return typeof c === "function" ? c(params) : c;
}

/** Checks that every parameter is present and within its allowed values. */
export function validateParams(templateId: string, params: RuleParams): string[] {
  const t = templateById(templateId);
  const errors: string[] = [];
  for (const [key, spec] of Object.entries(t.params)) {
    const v = params[key];
    if (v === undefined) {
      errors.push(`missing ${key}`);
    } else if (spec.kind === "choice" && !spec.options.includes(String(v))) {
      errors.push(`${key} must be one of ${spec.options.join(", ")}`);
    } else if (spec.kind === "number") {
      const n = Number(v);
      if (!Number.isFinite(n) || n < spec.min || n > spec.max) errors.push(`${key} must be ${spec.min}-${spec.max}`);
    }
  }
  return errors;
}

export type ArbiterVerdict = { accepted: true; cruel: boolean } | { accepted: false; reason: string };

/**
 * The Arbiter's deterministic checks (the AI only writes Kessa's announcement).
 * Rejects: bad parameters, the rule cap, duplicates, and direct contradictions.
 */
export function arbiterCheck(templateId: string, params: RuleParams, active: Rule[]): ArbiterVerdict {
  const errors = validateParams(templateId, params);
  if (errors.length) return { accepted: false, reason: `Invalid rule: ${errors.join("; ")}` };
  if (active.length >= MAX_ACTIVE_RULES) return { accepted: false, reason: `The Rite already holds ${MAX_ACTIVE_RULES} rules.` };

  const same = (r: Rule, keys: string[]) => keys.every((k) => String(r.params[k]) === String(params[k]));
  for (const r of active) {
    if (r.templateId === templateId && same(r, Object.keys(params))) {
      return { accepted: false, reason: `That rule is already in force: "${renderRule(r.templateId, r.params)}".` };
    }
    // "PvP rewards in Ward X" vs "No PvP in Ward X"
    if (templateId === "R1" && params.action === "pvp" && r.templateId === "R8" && same(r, ["ward"])) {
      return { accepted: false, reason: `Contradicts "${renderRule(r.templateId, r.params)}": PvP is banned in that Ward.` };
    }
    if (templateId === "R8" && r.templateId === "R1" && r.params.action === "pvp" && same(r, ["ward"])) {
      return { accepted: false, reason: `Contradicts "${renderRule(r.templateId, r.params)}": PvP is rewarded in that Ward.` };
    }
    // "Action rewarded in Ward X" vs "Action forbidden in Ward X"
    if (templateId === "R1" && r.templateId === "R2" && same(r, ["action", "ward"])) {
      return { accepted: false, reason: `Contradicts "${renderRule(r.templateId, r.params)}": that action is forbidden there.` };
    }
    if (templateId === "R2" && r.templateId === "R1" && same(r, ["action", "ward"])) {
      return { accepted: false, reason: `Contradicts "${renderRule(r.templateId, r.params)}": that action is rewarded there.` };
    }
    // Only one Ward can host Calamities
    if (templateId === "R11" && r.templateId === "R11") {
      return { accepted: false, reason: `Contradicts "${renderRule(r.templateId, r.params)}": Calamities are already bound to one Ward.` };
    }
  }
  return { accepted: true, cruel: isCruel(templateId, params) };
}

export function wardsTouched(params: RuleParams): WardId[] {
  const w = params.ward;
  return typeof w === "string" && (WARDS as readonly string[]).includes(w) ? [w as WardId] : [...WARDS];
}

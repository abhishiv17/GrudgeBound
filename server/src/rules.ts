/**
 * Rules through the rite contract (P2-C5).
 *
 *   POST /rules/propose { account, templateId, params } -> Kessa's verdict now; the chain part runs behind it:
 *        the player's propose_rule is relayed (their points pay), then the game master resolves it as the Arbiter.
 *        An accepted rule joins world.rules once resolved, so prices and points change for everyone.
 *   GET /chain?rule=<jobId> -> the receipt (rule id on chain, explorer links).
 *   GET /rules?account=  -> templates with display labels, rules in force, the cost and the player's points (Rules panel).
 *
 * On start the server loads the rules in force from the contract (syncFromChain), so a restart keeps them.
 * Agents' rules stay in the simulation until they get their own NEAR accounts (Phase 3).
 */
import { randomUUID } from "node:crypto";
import {
  arbiterCheck, enactRule, log, paramLabel, renderRule, ruleCost, RULE_TEMPLATES, validateParams,
  type RuleParams, type World,
} from "@grudgebound/shared";
import { isAccountId, type ChainReceipt, type ChainRule, type ChainWriter } from "./chain";

type Answer<T> = { status: 200; body: T } | { status: number; body: { error: string } };

export interface ProposeAnswer {
  jobId: string;
  accepted: boolean;
  /** Kessa's reason, also stored on chain with the verdict. */
  reason: string;
  /** The rule in words. */
  text: string;
  cost: number;
  chain: ChainReceipt;
}

export interface TemplateParamView {
  key: string;
  /** Every value the parameter may take, with the words shown for it ("lantern" -> "Lantern Ward"). */
  options: { value: string; label: string }[];
}

export interface TemplateView {
  id: string;
  /** With {key} placeholders, e.g. "No PvP in {ward}". */
  text: string;
  params: TemplateParamView[];
  cruel: boolean;
}

export interface RulesOverview {
  /** False when the server runs off-chain: proposals are refused. */
  chain: boolean;
  cost: number;
  /** The player's points on the rite contract; null off-chain or without an account. */
  points: number | null;
  templates: TemplateView[];
  active: { id: number; text: string; author: string; cost: number }[];
}

/** The hackathon's rule templates, every parameter value spelled out (a number range becomes its values). */
export function templateCatalog(): TemplateView[] {
  return RULE_TEMPLATES.filter((t) => t.mvp).map((t) => ({
    id: t.id,
    text: t.text,
    cruel: typeof t.cruel === "boolean" ? t.cruel : false,
    params: Object.entries(t.params).map(([key, spec]) => {
      const values = spec.kind === "choice"
        ? spec.options.map(String)
        : Array.from({ length: spec.max - spec.min + 1 }, (_, i) => String(spec.min + i));
      return { key, options: values.map((v) => ({ value: v, label: paramLabel(key, v) })) };
    }),
  }));
}

/** Everything the Rules panel draws. */
export async function rulesOverview(world: World, chain: ChainWriter | null, account: string | null): Promise<RulesOverview> {
  let cost = ruleCost(world.rules.length);
  let points: number | null = null;
  if (chain) {
    try {
      cost = await chain.ruleCost();
      if (account && isAccountId(account)) points = await chain.points(account);
    } catch {
      // The panel still opens; the propose call reports the chain problem.
    }
  }
  return {
    chain: chain !== null,
    cost,
    points,
    templates: templateCatalog(),
    active: world.rules.map((r) => ({ id: r.id, text: renderRule(r.templateId, r.params), author: r.author, cost: r.cost })),
  };
}

/** "afreen.grudgebound.testnet" -> "Afreen" for Kessa's lines. */
export function displayName(account: string): string {
  const first = account.split(".")[0] ?? account;
  return first.charAt(0).toUpperCase() + first.slice(1);
}

function parseParams(raw: unknown): RuleParams | null {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return null;
  const out: RuleParams = {};
  for (const [k, v] of Object.entries(raw as Record<string, unknown>)) {
    if (typeof v !== "string" && typeof v !== "number") return null;
    out[k] = v;
  }
  return out;
}

export class ChainRules {
  /** Rite rule ids already in world.rules. */
  private readonly enacted = new Set<number>();

  constructor(
    private readonly world: World,
    private readonly chain: ChainWriter,
    /** Can the server sign for this account? (custodial demo players) */
    private readonly canSignFor: (account: string) => boolean,
  ) {}

  /** Puts a rule that is in force on chain into the world, once. Returns false for rules it can't read. */
  enact(r: Pick<ChainRule, "id" | "template_id" | "params" | "author" | "cost">): boolean {
    if (this.enacted.has(r.id)) return false;
    if (!RULE_TEMPLATES.some((t) => t.id === r.template_id)) return false;
    let params: RuleParams | null;
    try { params = parseParams(JSON.parse(r.params)); } catch { params = null; }
    if (!params || validateParams(r.template_id, params).length) return false;
    this.enacted.add(r.id);
    enactRule(this.world, { author: r.author, authorName: displayName(r.author), templateId: r.template_id, params, cost: r.cost });
    return true;
  }

  /** Loads the rules in force from the rite contract. */
  async syncFromChain(): Promise<number> {
    let n = 0;
    for (const r of await this.chain.activeRules()) if (this.enact(r)) n++;
    return n;
  }

  async propose(body: unknown): Promise<Answer<ProposeAnswer>> {
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    const account = b.account;
    if (!isAccountId(account)) return fail(400, "account must be a NEAR account id");
    if (!this.canSignFor(account)) return fail(403, `the server can't sign for ${account} (only its demo players until email sign-in)`);
    const templateId = b.templateId;
    if (typeof templateId !== "string" || !RULE_TEMPLATES.some((t) => t.id === templateId)) return fail(400, "unknown rule template");
    const raw = parseParams(b.params);
    if (!raw) return fail(400, "params must be an object of strings and numbers");
    // Clients may send every value as text; number parameters are stored as numbers (the engine does maths on them).
    const spec = RULE_TEMPLATES.find((t) => t.id === templateId)!.params;
    const params: RuleParams = Object.fromEntries(Object.entries(raw).map(([k, v]) =>
      [k, spec[k]?.kind === "number" && typeof v === "string" && v.trim() !== "" && Number.isFinite(Number(v)) ? Number(v) : v]));
    // Malformed rules never reach the chain; contradictions and duplicates do, so the rejection is on record.
    const errors = validateParams(templateId, params);
    if (errors.length) return fail(400, `Invalid rule: ${errors.join("; ")}`);

    let points: number;
    let cost: number;
    try {
      [points, cost] = await Promise.all([this.chain.points(account), this.chain.ruleCost()]);
    } catch (e) {
      return fail(502, `could not read the rite contract: ${e instanceof Error ? e.message : String(e)}`);
    }
    if (points < cost) return fail(402, `A rule costs ${cost} points; ${displayName(account)} has ${points}.`);

    const verdict = arbiterCheck(templateId, params, this.world.rules);
    const accepted = verdict.accepted;
    const reason = verdict.accepted
      ? verdict.cruel ? "Accepted. The Wards it touches will not forget it." : "Accepted."
      : verdict.reason;
    const text = renderRule(templateId, params);
    const jobId = randomUUID();
    const name = displayName(account);

    const chain = await this.chain.proposeRule({ jobId, account, templateId, params, accepted, reason }, (chainRuleId) => {
      if (accepted) this.enact({ id: chainRuleId, template_id: templateId, params: JSON.stringify(params), author: account, cost });
      else log(this.world, { type: "rule-rejected", actor: account, text: `Kessa rejects ${name}'s rule "${text}": ${reason} (points refunded)` });
    });
    return { status: 200, body: { jobId, accepted, reason, text, cost, chain } };
  }
}

function fail(status: number, error: string): { status: number; body: { error: string } } {
  return { status, body: { error } };
}

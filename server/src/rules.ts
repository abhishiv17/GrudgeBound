/**
 * Rules through the rite contract (P2-C5).
 *
 *   POST /rules/propose { account, templateId, params } -> Kessa's verdict now; the chain part runs behind it:
 *        the player's propose_rule is relayed (their points pay), then the game master resolves it as the Arbiter.
 *        An accepted rule joins world.rules once resolved, so prices and points change for everyone.
 *   GET /chain?rule=<jobId> -> the receipt (rule id on chain, explorer links).
 *   POST /rules/repeal { account, ruleId } -> relays the player's repeal_rule (twice the rule's cost, paid on chain);
 *        the rule leaves world.rules once the contract has repealed it. ruleId is the rule's id on the rite contract.
 *   GET /rules?account=  -> templates with display labels, rules in force, the cost and the player's points (Rules panel).
 *
 * On start the server loads the rules in force from the contract (syncFromChain), so a restart keeps them.
 * Agents' rules stay in the simulation until they get their own NEAR accounts (Phase 3).
 */
import { randomUUID } from "node:crypto";
import {
  arbiterCheck, enactRule, repealRule, log, REPEAL_MULTIPLIER, paramLabel, renderRule, ruleCost, RULE_TEMPLATES, validateParams,
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

export interface RepealAnswer {
  jobId: string;
  /** The rule's id on the rite contract. */
  ruleId: number;
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
  /**
   * id: the rule's number in the world (Kessa's "#N"); chainId: its id on the rite contract (null for a rule
   * only the simulation knows), which POST /rules/repeal takes; repealCost: what striking it costs.
   */
  active: { id: number; chainId: number | null; text: string; author: string; cost: number; repealCost: number }[];
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
export async function rulesOverview(
  world: World, chain: ChainWriter | null, account: string | null, chainRules: ChainRules | null = null,
): Promise<RulesOverview> {
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
    active: world.rules.map((r) => ({
      id: r.id, chainId: chainRules?.chainIdOf(r.id) ?? null, text: renderRule(r.templateId, r.params),
      author: r.author, cost: r.cost, repealCost: r.cost * REPEAL_MULTIPLIER,
    })),
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
  /** Rite rule id -> world rule id, for every chain rule in world.rules. */
  private readonly enacted = new Map<number, number>();
  /** Rite rule ids with a repeal on its way, so the same rule isn't paid for twice. */
  private readonly repealing = new Set<number>();

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
    const rule = enactRule(this.world, { author: r.author, authorName: displayName(r.author), templateId: r.template_id, params, cost: r.cost });
    this.enacted.set(r.id, rule.id);
    return true;
  }

  /** The rite contract's id for a world rule, or null when the rule came from the simulation. */
  chainIdOf(worldRuleId: number): number | null {
    for (const [chainId, worldId] of this.enacted) if (worldId === worldRuleId) return chainId;
    return null;
  }

  /** Takes a repealed chain rule out of the world, once. */
  private strike(chainId: number, by: string, cost: number): boolean {
    const worldId = this.enacted.get(chainId);
    if (worldId === undefined) return false;
    this.enacted.delete(chainId);
    return repealRule(this.world, worldId, { by, byName: displayName(by), cost }) !== null;
  }

  /**
   * Matches world.rules to the rules in force on the rite contract: adds new ones, and strikes ones that were
   * repealed there by someone else. Returns how many were added.
   */
  async syncFromChain(): Promise<number> {
    const onChain = await this.chain.activeRules();
    let n = 0;
    for (const r of onChain) if (this.enact(r)) n++;
    const live = new Set(onChain.map((r) => r.id));
    for (const chainId of [...this.enacted.keys()]) {
      if (!live.has(chainId) && !this.repealing.has(chainId)) this.strike(chainId, "the Rite", 0);
    }
    return n;
  }

  async repeal(body: unknown): Promise<Answer<RepealAnswer>> {
    const b = (body && typeof body === "object" ? body : {}) as Record<string, unknown>;
    const account = b.account;
    if (!isAccountId(account)) return fail(400, "account must be a NEAR account id");
    if (!this.canSignFor(account)) return fail(403, `the server can't sign for ${account} (only its demo players until email sign-in)`);
    const ruleId = typeof b.ruleId === "string" && b.ruleId.trim() !== "" ? Number(b.ruleId) : b.ruleId;
    if (typeof ruleId !== "number" || !Number.isInteger(ruleId) || ruleId < 0) return fail(400, "ruleId must be the rule's id on the rite contract");
    const worldId = this.enacted.get(ruleId);
    const rule = worldId === undefined ? undefined : this.world.rules.find((r) => r.id === worldId);
    if (!rule) return fail(404, `rule ${ruleId} is not in force`);
    if (this.repealing.has(ruleId)) return fail(409, `rule ${ruleId} is already being repealed`);
    const cost = rule.cost * REPEAL_MULTIPLIER;

    let points: number;
    try {
      points = await this.chain.points(account);
    } catch (e) {
      return fail(502, `could not read the rite contract: ${e instanceof Error ? e.message : String(e)}`);
    }
    if (points < cost) return fail(402, `Repealing rule #${rule.id} costs ${cost} points; ${displayName(account)} has ${points}.`);

    const text = renderRule(rule.templateId, rule.params);
    const jobId = randomUUID();
    this.repealing.add(ruleId);
    const chain = await this.chain.repealRule(
      { jobId, account, ruleId },
      () => { this.repealing.delete(ruleId); this.strike(ruleId, account, cost); },
      () => { this.repealing.delete(ruleId); }, // the rule stays in force; the player may try again
    );
    return { status: 200, body: { jobId, ruleId, text, cost, chain } };
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

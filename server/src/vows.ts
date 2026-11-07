/** P3-C4: authenticated stakes and settlement for the Unreal Vow Shrine.
 * Restrictions are enforced before gameplay actions; releasing a vow burns its stake.
 * Deadlines come from the contract, so restarting the game/server cannot reset them.
 */
import { randomUUID } from "node:crypto";
import { VOW_TEMPLATES, WARDS, WARD_INFO, log, type Omen, type PlayerAction, type WardId, type World } from "@grudgebound/shared";
import { AuthError } from "./auth";
import type { BattleContext } from "./battles";
import type { ChainReceipt, ChainVow, ChainWriter } from "./chain";

export interface VowView {
  id: number; templateId: string; text: string; stake: number; status: string; endsAt: number; payout: number;
}
export interface VowsOverview {
  chain: boolean; points: number | null; daySeconds: number;
  templates: { id: string; text: string; stake: number; minDays: number; maxDays: number }[];
  talismans: { id: string; name: string }[];
  vows: VowView[];
}

function parameters(vow: ChainVow): Record<string, string> {
  const p: unknown = JSON.parse(vow.params);
  if (!p || typeof p !== "object" || Array.isArray(p)) throw new Error("Invalid vow parameters in the Rite");
  return p as Record<string, string>;
}

function text(v: ChainVow): string {
  const template = VOW_TEMPLATES.find((t) => t.id === v.template_id);
  const p = parameters(v);
  return (template?.text ?? v.template_id).replace(/\{(\w+)\}/g, (_, key: string) => {
    const value = p[key] ?? "?";
    return key === "ward" ? WARD_INFO[value as WardId]?.name ?? value : value;
  });
}

export class Vows {
  private readonly busy = new Set<string>();
  private readonly settling = new Set<number>();
  constructor(private readonly world: World, private readonly chain: ChainWriter | null,
    private readonly canSign: (account: string) => boolean, readonly tickMs: number, private readonly now = Date.now) {
    if (!Number.isFinite(tickMs) || tickMs <= 0) throw new Error("TICK_MS must be positive");
  }

  async snapshot(account: string): Promise<ChainVow[]> {
    if (!this.chain) return [];
    try { return await this.chain.vows(account); }
    catch { throw new AuthError(502, "The Rite's vows cannot be read. Please try again."); }
  }

  private active(vows: ChainVow[]): ChainVow[] {
    return vows.filter((v) => v.status === "Active" && v.ends_ms > this.now());
  }

  async overview(account: string): Promise<VowsOverview> {
    const [vows, points, owned] = this.chain ? await Promise.all([
      this.snapshot(account), this.chain.points(account), this.chain.talismansOf(account),
    ]) : [[], null, []] as const;
    return {
      chain: !!this.chain, points, daySeconds: this.tickMs * 24 / 1000,
      // Combat/purse power bonuses are not advertised until implemented; the stake bonus is real today.
      templates: VOW_TEMPLATES.filter((t) => t.mvp).map((t) => ({ id: t.id, text: t.text, stake: t.stake, minDays: t.durationDays[0], maxDays: t.durationDays[1] })),
      talismans: owned.map((t) => ({ id: t.token_id, name: t.meta.species })),
      vows: vows.map((v) => ({ id: v.id, templateId: v.template_id, text: text(v), stake: v.stake,
        status: v.status === "Active" && v.ends_ms <= this.now() ? "Settling" : v.status,
        endsAt: v.ends_ms, payout: v.stake + Math.floor(v.stake * 20 / 100) })),
    };
  }

  async swear(account: string, body: Record<string, unknown>): Promise<{ jobId: string; chain: ChainReceipt }> {
    if (!this.chain || !this.canSign(account)) throw new AuthError(503, "The Rite is not ready to accept vows for this player.");
    if (this.busy.has(account)) throw new AuthError(409, "Your previous vow is still being recorded.");
    this.busy.add(account);
    let queued = false;
    try {
      const template = VOW_TEMPLATES.find((t) => t.id === body.templateId && t.mvp);
      if (!template) throw new AuthError(400, "Choose one of the Shrine's vows.");
      const days = body.days;
      if (!Number.isInteger(days) || (days as number) < template.durationDays[0] || (days as number) > template.durationDays[1]) throw new AuthError(400, "Choose a valid vow duration.");
      const raw = body.params;
      if (!raw || typeof raw !== "object" || Array.isArray(raw)) throw new AuthError(400, "Vow parameters must be an object.");
      const p = raw as Record<string, unknown>;
      const params: Record<string, string> = {};
      if (template.id === "V1") {
        if (!(WARDS as readonly unknown[]).includes(p.ward)) throw new AuthError(400, "Choose a Ward.");
        params.ward = p.ward as string;
      }
      if (template.id === "V3") {
        const owned = await this.chain.talismansOf(account);
        if (typeof p.talisman !== "string" || !owned.some((t) => t.token_id === p.talisman)) throw new AuthError(403, "Choose a talisman you own.");
        params.talisman = p.talisman;
      }
      if (template.id === "V4") {
        if (!["whisper", "wail", "calamity"].includes(String(p.omen))) throw new AuthError(400, "Choose a curse's omen.");
        params.omen = p.omen as string;
      }
      const vows = await this.snapshot(account);
      if (vows.some((v) => v.status === "Active" && v.template_id === template.id)) throw new AuthError(409, "You already have this vow; wait for it to settle.");
      if (await this.chain.points(account) < template.stake) throw new AuthError(409, `This vow needs ${template.stake} points to stake.`);
      const jobId = randomUUID();
      const chain = await this.chain.job("vow", jobId, account, async ({ relay, tx, receipt }) => {
        const r = await relay(account, this.chain!.cfg.riteContract, "make_vow", {
          template_id: template.id, params: JSON.stringify(params), stake: template.stake, duration_ms: (days as number) * 24 * this.tickMs,
        });
        tx("swear", r.hash);
        const id = Number(r.result);
        if (!Number.isSafeInteger(id) || id < 0) throw new Error("The Rite returned an invalid vow id");
        receipt.vowId = id;
      }, () => { this.busy.delete(account); }, () => { this.busy.delete(account); });
      queued = true;
      return { jobId, chain };
    } finally { if (!queued) this.busy.delete(account); }
  }

  async release(account: string, id: unknown): Promise<{ jobId: string; chain: ChainReceipt }> {
    if (!this.chain) throw new AuthError(503, "The Rite is unavailable.");
    if (!Number.isSafeInteger(id) || (id as number) < 0) throw new AuthError(400, "Choose a vow.");
    const vow = (await this.snapshot(account)).find((v) => v.id === id && v.owner === account);
    if (!vow) throw new AuthError(404, "This vow does not belong to you.");
    if (vow.status !== "Active") throw new AuthError(409, "This vow has already settled.");
    if (vow.ends_ms <= this.now()) throw new AuthError(409, "This vow has ended; its stake is being returned.");
    return this.settle(vow, false);
  }

  /** No client may choose `kept`: the server waits for the contract's deadline. */
  async sweep(accounts: string[]): Promise<void> {
    if (!this.chain) return;
    for (const account of new Set(accounts)) {
      for (const v of await this.snapshot(account)) {
        if (v.status === "Active" && v.ends_ms <= this.now() && !this.settling.has(v.id)) await this.settle(v, true);
      }
    }
  }

  private async settle(vow: ChainVow, kept: boolean) {
    if (this.settling.has(vow.id)) throw new AuthError(409, "This vow is already settling.");
    this.settling.add(vow.id);
    const jobId = randomUUID();
    const chain = await this.chain!.job("vow", jobId, vow.owner, async ({ call, tx, receipt }) => {
      // Recheck inside the shared writer queue: another server may have settled it already.
      const current = (await this.snapshot(vow.owner)).find((v) => v.id === vow.id);
      if (!current || current.status !== "Active") throw new Error("This vow has already settled");
      if (kept && current.ends_ms > this.now()) throw new Error("This vow has not ended");
      const r = await call(this.chain!.cfg.riteContract, "settle_vow", { vow_id: vow.id, kept });
      receipt.vowId = vow.id; tx("settle", r.hash);
    }, () => {
      this.settling.delete(vow.id);
      if (!kept) {
        const ward = parameters(vow).ward as WardId | undefined;
        const grudge = this.world.wards[ward && WARDS.includes(ward) ? ward : "lantern"].grudge;
        grudge.dread = Math.min(100 - grudge.rage - grudge.sorrow, grudge.dread + 4);
      }
      log(this.world, { type: "chain", actor: vow.owner, text: kept ? "The Rite returned a kept vow's stake with a fifth added." : "A released vow's stake burned. Dread stirs in the Ward." });
    }, () => { this.settling.delete(vow.id); });
    return { jobId, chain };
  }

  battleGuard(vows: ChainVow[], context: BattleContext, action?: PlayerAction): string | null {
    for (const v of this.active(vows)) {
      const p = parameters(v);
      if (v.template_id === "V1" && p.ward !== context.ward) return "Your vow keeps you in its Ward. Release it at the Shrine first.";
      if (v.template_id === "V2" && action?.kind === "item") return "Your vow forbids healing items. Release it at the Shrine first.";
      if (v.template_id === "V4" && action && action.kind !== "flee" && p.omen !== context.omen) return "Your vow allows another omen. Flee, or release it at the Shrine before fighting.";
    }
    return null;
  }

  assertTrade(vows: ChainVow[], ward: WardId, tokenId?: unknown) {
    const refusal = this.battleGuard(vows, { ward, omen: "whisper" as Omen });
    if (refusal) throw new AuthError(409, refusal);
    if (this.active(vows).some((v) => v.template_id === "V3" && parameters(v).talisman === tokenId)) throw new AuthError(409, "You swore not to sell this talisman. Release that vow at the Shrine first.");
  }
}

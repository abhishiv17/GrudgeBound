/**
 * Contract events (NEP-297) into the town's event feed (P2-C6).
 *
 * Both contracts log `EVENT_JSON:{standard, version, event, data: [...]}` for every change. Every write today goes
 * through this server (signed by the game master, or relayed for a player), so the server reads the events from
 * the outcome of each transaction it sends and puts them in GET /events with an explorer link: Kessa's town log
 * then shows what the chain recorded, in the same words as everything else.
 * Writes made elsewhere (a player calling the contract with their own wallet) need a block indexer: Phase 3.
 */
import { displayName } from "./rules";

/** One NEP-297 event as logged by a contract. */
export interface RawChainEvent {
  /** The contract that logged it. */
  contract: string;
  standard: string;
  event: string;
  data: Record<string, unknown>[];
}

/** A raw event plus the transaction it came from. */
export interface ChainEvent extends RawChainEvent {
  txHash: string;
  url: string;
}

/** Outcome shape we read: what near-api-js returns for a sent transaction. */
interface OutcomeLike {
  receipts_outcome: { outcome: { executor_id: string; logs: string[] } }[];
}

/** Every EVENT_JSON log in a transaction's receipts, in execution order. Malformed logs are skipped. */
export function eventsIn(res: OutcomeLike): RawChainEvent[] {
  const out: RawChainEvent[] = [];
  for (const r of res.receipts_outcome ?? []) {
    for (const line of r.outcome.logs ?? []) {
      if (!line.startsWith("EVENT_JSON:")) continue;
      try {
        const e = JSON.parse(line.slice("EVENT_JSON:".length)) as Partial<RawChainEvent>;
        if (typeof e.standard !== "string" || typeof e.event !== "string") continue;
        out.push({ contract: r.outcome.executor_id, standard: e.standard, event: e.event, data: Array.isArray(e.data) ? e.data : [] });
      } catch { /* not ours to fix */ }
    }
  }
  return out;
}

const str = (v: unknown) => (typeof v === "string" ? v : v === undefined || v === null ? "" : String(v));

/**
 * Kessa's words for a chain event, or null for ones the town needn't hear about.
 * `game` is the game master's account (it holds the merchants' purses and the treasury).
 */
export function describeChainEvent(e: RawChainEvent, game: string): { type: string; actor?: string; text: string } | null {
  const who = (account: unknown) => {
    const a = str(account);
    if (a === game) return "the game master";
    return a.endsWith(`.${game}`) ? displayName(a) : a;
  };
  const d = e.data[0] ?? {};
  switch (e.event) {
    case "mt_mint": {
      const token = str((d.token_ids as unknown[] | undefined)?.[0]);
      const amount = str((d.amounts as unknown[] | undefined)?.[0]);
      if (token === "ember") return { type: "chain", actor: str(d.owner_id), text: `NEAR: ${amount} Ember minted to ${who(d.owner_id)}.` };
      return { type: "chain", actor: str(d.owner_id), text: `NEAR: talisman ${token} sealed for ${who(d.owner_id)}.` };
    }
    case "mt_transfer": {
      const token = str((d.token_ids as unknown[] | undefined)?.[0]);
      const amount = str((d.amounts as unknown[] | undefined)?.[0]);
      const what = token === "ember" ? `${amount} Ember` : `talisman ${token}`;
      return { type: "chain", actor: str(d.old_owner_id), text: `NEAR: ${who(d.old_owner_id)} sent ${what} to ${who(d.new_owner_id)}.` };
    }
    case "battle":
      return { type: "chain", text: `NEAR: the Rite recorded battle ${str(d.battle_id).slice(0, 8)} (seed ${str(d.seed)}).` };
    case "rule_proposed":
      return {
        type: "chain", actor: str(d.author),
        text: `NEAR: ${who(d.author)} paid ${str(d.cost)} points to propose rule ${str(d.id)} (${str(d.template_id)}).`,
      };
    case "rule_resolved":
      return {
        type: "chain",
        text: d.accepted === true
          ? `NEAR: rule ${str(d.id)} is law.`
          : `NEAR: rule ${str(d.id)} rejected${d.reason ? `: ${str(d.reason)}` : ""} (points refunded).`,
      };
    case "rule_repealed":
      return { type: "chain", actor: str(d.by), text: `NEAR: ${who(d.by)} paid ${str(d.cost)} points to repeal rule ${str(d.id)}.` };
    case "vow_made":
      return { type: "chain", actor: str(d.owner), text: `NEAR: ${who(d.owner)} swore vow ${str(d.id)} (${str(d.template_id)}), staking ${str(d.stake)} points.` };
    case "vow_settled":
      return { type: "chain", text: `NEAR: vow ${str(d.id)} ${d.kept === true ? "kept" : "broken"}.` };
    default:
      return null;
  }
}

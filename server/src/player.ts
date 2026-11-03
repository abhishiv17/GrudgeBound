/**
 * GET /player?account=<id> — a player as the chain sees them (P2-G5): Rite points, Ember, and every talisman
 * they own on the items contract. The Talisman Case and the points line read this instead of the local save,
 * so a talisman bought, sold or traded anywhere shows up (or goes) everywhere.
 */
import { isAccountId, type ChainTalisman, type ChainWriter } from "./chain";
import { displayName } from "./rules";

export interface PlayerTalisman {
  tokenId: string;
  speciesId: string;
  emotion: string;
  omen: string;
  level: number;
  wins: number;
  /** Ember the talisman has earned (bounties); travels with it when traded. */
  purse: number;
}

export interface PlayerView {
  account: string;
  name: string;
  /** False when the server runs off-chain: the client keeps its local save. */
  chain: boolean;
  points: number | null;
  ember: number | null;
  talismans: PlayerTalisman[];
  /** The account on the block explorer. */
  explorerUrl: string | null;
}

export function explorerAccountUrl(network: string, account: string): string {
  return network === "mainnet" ? `https://nearblocks.io/address/${account}` : `https://${network}.nearblocks.io/address/${account}`;
}

export function toPlayerTalisman(t: ChainTalisman): PlayerTalisman {
  return {
    tokenId: t.token_id,
    speciesId: t.meta.species,
    emotion: t.meta.emotion,
    omen: t.meta.omen,
    level: t.meta.level,
    wins: t.meta.wins,
    purse: Number(t.purse ?? 0),
  };
}

type Answer = { status: 200; body: PlayerView } | { status: number; body: { error: string } };

export async function playerView(chain: ChainWriter | null, account: unknown): Promise<Answer> {
  if (!isAccountId(account)) return { status: 400, body: { error: "account must be a NEAR account id" } };
  const base: PlayerView = { account, name: displayName(account), chain: false, points: null, ember: null, talismans: [], explorerUrl: null };
  if (!chain) return { status: 200, body: base };
  try {
    // Three reads at once; they don't go through the write queue.
    const [points, ember, talismans] = await Promise.all([chain.points(account), chain.ember(account), chain.talismansOf(account)]);
    return {
      status: 200,
      body: { ...base, chain: true, points, ember, talismans: talismans.map(toPlayerTalisman), explorerUrl: explorerAccountUrl(chain.cfg.network, account) },
    };
  } catch (e) {
    return { status: 502, body: { error: `could not read the chain: ${(e as Error).message}` } };
  }
}

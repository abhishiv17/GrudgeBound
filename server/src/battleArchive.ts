/**
 * Every battle the Rite paid for, kept so anyone can replay it (P2-A2).
 *
 * NEAR stores each battle's seed and a hash of its log (submit_battle), not the moves. The server appends the
 * full replay record here when a battle ends, and adds the on-chain hash and transaction when the rite contract's
 * `battle` event comes back. `npm run replay <seed|battleId>` (replay.ts) re-runs the battle from this record,
 * recomputes the hash and checks it against the one on NEAR.
 *
 * One JSON object per line in data/battles.jsonl (BATTLE_ARCHIVE to move it). Kept out of git: it's runtime data.
 */
import { appendFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { battleLogHash, type BattleRecord } from "./chain";

export interface ArchivedBattle extends BattleRecord {
  ward: string;
  /** When it ended (ms since epoch). */
  at: number;
  /** sha256 of the replay record, as computed by the server (battleLogHash). */
  logHash: string;
  /** Filled in when the rite contract logs the battle. */
  chain?: { logHash: string; txHash: string; url: string };
}

type Line = { kind: "battle"; battle: ArchivedBattle } | { kind: "chain"; battleId: string; logHash: string; txHash: string; url: string };

export const DEFAULT_ARCHIVE = resolve(process.cwd(), "data", "battles.jsonl");

export class BattleArchive {
  constructor(readonly file: string = process.env.BATTLE_ARCHIVE || DEFAULT_ARCHIVE) {}

  add(record: BattleRecord, ward: string, at = Date.now()): ArchivedBattle {
    const battle: ArchivedBattle = { ...record, ward, at, logHash: battleLogHash(record) };
    this.write({ kind: "battle", battle });
    return battle;
  }

  /** The rite contract logged this battle: remember the hash it stored and where. */
  attachChain(battleId: string, logHash: string, txHash: string, url: string): void {
    this.write({ kind: "chain", battleId, logHash, txHash, url });
  }

  /** Every archived battle, oldest first, with its chain entry merged in. */
  all(): ArchivedBattle[] {
    if (!existsSync(this.file)) return [];
    const byId = new Map<string, ArchivedBattle>();
    for (const raw of readFileSync(this.file, "utf8").split("\n")) {
      if (!raw.trim()) continue;
      let line: Line;
      try { line = JSON.parse(raw) as Line; } catch { continue; }
      if (line.kind === "battle") byId.set(line.battle.battleId, line.battle);
      else {
        const b = byId.get(line.battleId);
        if (b) b.chain = { logHash: line.logHash, txHash: line.txHash, url: line.url };
      }
    }
    return [...byId.values()];
  }

  /** By seed (a number) or by battle id (or the start of one); the latest match wins. */
  find(key: string): ArchivedBattle | undefined {
    const all = this.all();
    const asSeed = /^\d+$/.test(key) ? Number(key) : null;
    return all.reverse().find((b) => (asSeed !== null && b.seed === asSeed) || b.battleId === key || (key.length >= 6 && b.battleId.startsWith(key)));
  }

  private write(line: Line) {
    mkdirSync(dirname(this.file), { recursive: true });
    appendFileSync(this.file, JSON.stringify(line) + "\n");
  }
}

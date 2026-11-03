import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { createWorld } from "@grudgebound/shared";
import { BattleSessions, type TurnAnswer } from "./battles";
import { BattleArchive } from "./battleArchive";
import type { BattleRecord } from "./chain";
import { checkBattle } from "./replay";

/** Fights a server battle to the end (attacking) and returns its replay record. */
function finishedBattle(seed: number): BattleRecord | null {
  const sessions = new BattleSessions(createWorld(7), { newSeed: () => seed, newId: () => `battle-${seed}` });
  sessions.start({ ward: "lantern", by: "Tester", account: "afreen.gb.testnet" });
  for (let i = 0; i < 200; i++) {
    const t = sessions.turn({ battleId: `battle-${seed}`, action: { kind: "attack" } }).body as TurnAnswer;
    if (!t.outcome) continue;
    if (t.outcome !== "won") return null;
    return {
      battleId: t.battleId, account: t.account!, outcome: "won", points: t.points!, seed: t.seed!, actions: t.actions!,
      wild: { speciesId: t.view.wild.speciesId, emotion: t.view.wild.emotion, omen: t.view.wild.omen }, playerCurse: t.view.player.speciesId,
    };
  }
  return null;
}

describe("npm run replay (P2-A2)", () => {
  const dir = mkdtempSync(join(tmpdir(), "gb-archive-"));
  const archive = new BattleArchive(join(dir, "battles.jsonl"));
  let record: BattleRecord | null = null;
  for (let seed = 1; seed < 80 && !record; seed++) record = finishedBattle(seed);

  test("an archived server battle replays to the same ending and hash", () => {
    assert.ok(record, "some seed gives a win");
    archive.add(record, "lantern", 1);
    const b = archive.find(String(record.seed))!;
    assert.equal(b.battleId, record.battleId);
    const r = checkBattle(b);
    assert.equal(r.outcomeMatches, true);
    assert.equal(r.hashMatches, true);
    assert.match(r.lines.at(-1)!, /.+/);
  });

  test("the chain's hash is merged in, and a tampered record fails", () => {
    archive.attachChain(record!.battleId, "abc", "TX", "https://testnet.nearblocks.io/txns/TX");
    const b = archive.find(record!.battleId.slice(0, 8))!;
    assert.deepEqual(b.chain, { logHash: "abc", txHash: "TX", url: "https://testnet.nearblocks.io/txns/TX" });
    const forged = { ...b, actions: b.actions.slice(0, -1) };
    assert.equal(checkBattle(forged).outcomeMatches, false, "one move fewer doesn't end the battle");
    assert.equal(checkBattle(forged).hashMatches, false);
    assert.equal(archive.find("424242424"), undefined);
    rmSync(dir, { recursive: true, force: true });
  });
});

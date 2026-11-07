/**
 * npm run replay <seed | battleId>   (P2-A2)
 *
 * Re-checks a battle the Rite paid for: replays it from the archived seed and moves with the shared battle
 * engine, checks it ends the way the server recorded, recomputes the log hash, and (when the battle was written
 * to NEAR) reads the transaction back from the chain and compares the hash the rite contract logged.
 *
 *   npm run replay              -> lists the archived battles
 *   npm run replay 2718281828   -> replays the battle with that seed
 *   npm run replay 9f3c2a       -> ... or with that battle id (or its first 6+ characters)
 *
 * Exit code 0 when everything matches, 1 when something doesn't, 2 when the battle isn't found.
 */
import { JsonRpcProvider } from "near-api-js";
import { createBattle, curseById, describeEvent, introLine, replayBattle } from "@grudgebound/shared";
import { BattleArchive, type ArchivedBattle } from "./battleArchive";
import { battleLogHash, chainConfigFromEnv, defaultRpc } from "./chain";
import { eventsIn } from "./chainEvents";

export interface ReplayCheck {
  outcomeMatches: boolean;
  /** The hash recomputed from the archived record now. */
  logHash: string;
  /** Matches the hash the server archived when the battle ended. */
  hashMatches: boolean;
  lines: string[];
  finalOutcome: string | null;
}

/** Replays an archived battle and compares it with what was recorded. Pure: no chain access. */
export function checkBattle(b: ArchivedBattle): ReplayCheck {
  const setup = { wild: b.wild.speciesId, omen: b.wild.omen, playerCurse: b.playerCurse, ...b.initial };
  const turns = replayBattle(setup, b.seed, b.actions);
  const lines = [introLine(createBattle(setup))];
  for (const t of turns) for (const e of t.events) lines.push(describeEvent(e, t.state));
  const finalOutcome = turns.at(-1)?.state.outcome ?? null;
  const logHash = battleLogHash(b);
  return {
    outcomeMatches: finalOutcome === b.outcome && turns.length === b.actions.length,
    logHash,
    hashMatches: logHash === b.logHash,
    lines,
    finalOutcome,
  };
}

/** The log_hash the rite contract logged in that transaction (its `battle` event), or null. */
async function chainLogHash(b: ArchivedBattle): Promise<string | null> {
  if (!b.chain) return null;
  const cfg = chainConfigFromEnv();
  const network = cfg?.network ?? process.env.NEAR_NETWORK ?? "testnet";
  const provider = new JsonRpcProvider({ url: cfg?.rpcUrl ?? process.env.NEAR_RPC_URL ?? defaultRpc(network) });
  const sender = cfg?.gameAccount ?? process.env.NEAR_GAME_ACCOUNT ?? "grudgebound.testnet";
  const res = await provider.viewTransactionStatus({ txHash: b.chain.txHash, accountId: sender, waitUntil: "FINAL" });
  const ev = eventsIn(res).find((e) => e.event === "battle" && e.data[0]?.battle_id === b.battleId);
  const h = ev?.data[0]?.log_hash;
  return typeof h === "string" ? h : null;
}

async function main() {
  const archive = new BattleArchive();
  const key = process.argv[2];
  if (!key) {
    const all = archive.all();
    if (!all.length) {
      console.log(`No battles archived yet in ${archive.file}. Win or bind one against the server first.`);
      return;
    }
    console.log(`${all.length} archived battle(s) in ${archive.file}:\n`);
    for (const b of all) {
      console.log(`  seed ${String(b.seed).padStart(10)}  ${b.battleId.slice(0, 8)}  ${b.outcome.padEnd(5)}  ${curseById(b.wild.speciesId).name} vs ${curseById(b.playerCurse).name}  +${b.points}  ${b.account}${b.chain ? "  (on NEAR)" : ""}`);
    }
    console.log(`\nnpm run replay <seed or battle id>`);
    return;
  }

  const b = archive.find(key);
  if (!b) {
    console.error(`No archived battle with seed or id "${key}" (run npm run replay to list them).`);
    process.exitCode = 2;
    return;
  }
  console.log(`Battle ${b.battleId}  seed ${b.seed}  ${b.ward}  ${new Date(b.at).toISOString()}`);
  console.log(`${curseById(b.playerCurse).name} (${b.account}) vs ${curseById(b.wild.speciesId).name} (${b.wild.omen}), ${b.actions.length} moves\n`);

  const r = checkBattle(b);
  for (const line of r.lines) console.log(`  ${line}`);
  console.log("");
  let ok = true;
  const mark = (good: boolean, text: string) => { ok &&= good; console.log(`${good ? "OK  " : "FAIL"}  ${text}`); };
  mark(r.outcomeMatches, `replay ends "${r.finalOutcome}"; the server recorded "${b.outcome}" after ${b.actions.length} moves`);
  mark(r.hashMatches, `log hash ${r.logHash.slice(0, 16)}... matches the one archived when the battle ended`);
  if (b.chain) {
    try {
      const onChain = await chainLogHash(b);
      mark(onChain === r.logHash, `NEAR: the rite contract logged ${onChain ? `${onChain.slice(0, 16)}...` : "no hash"} (${b.chain.url})`);
    } catch (e) {
      mark(false, `could not read the transaction from NEAR: ${(e as Error).message}`);
    }
  } else {
    console.log("--    not written to NEAR (off-chain server, or 0 points)");
  }
  console.log(ok ? "\nVERIFIED: this battle happened exactly as the Rite recorded it." : "\nMISMATCH: see the FAIL lines above.");
  // exitCode, not exit(): exiting while the RPC socket closes trips a libuv assert on Windows.
  process.exitCode = ok ? 0 : 1;
}

// Run only as a script (tests import checkBattle).
if (process.argv[1] && /replay\.ts$/.test(process.argv[1])) void main();

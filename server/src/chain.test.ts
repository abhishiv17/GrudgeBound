import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { ChainWriter, battleLogHash, chainConfigFromEnv, explorerTxUrl, isAccountId, type BattleRecord, type ContractCall } from "./chain";
import { BattleSessions } from "./battles";
import { createWorld } from "@grudgebound/shared";

const CFG = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet" };

function record(over: Partial<BattleRecord> = {}): BattleRecord {
  return {
    battleId: "b1", account: "alice.testnet", outcome: "bound", points: 5, seed: 42,
    actions: [{ kind: "attack" }, { kind: "seal" }],
    wild: { speciesId: "hushmoth", emotion: "sorrow", omen: "whisper" }, playerCurse: "cinder-grin",
    ...over,
  };
}

/** Records every call; `fail` names a method that throws. */
function fakeChain(fail?: string, delayMs = 0) {
  const calls: { contractId: string; methodName: string; args: Record<string, unknown> }[] = [];
  let n = 0;
  const call: ContractCall = async (contractId, methodName, args) => {
    if (delayMs) await new Promise((r) => setTimeout(r, delayMs));
    if (methodName === fail) throw new Error(`${methodName} exploded`);
    calls.push({ contractId, methodName, args });
    n++;
    return { hash: `tx${n}`, result: methodName === "mint_talisman" ? `tal-${n}` : null };
  };
  return { calls, call };
}

describe("chain writer", () => {
  test("a binding mints the talisman to the player, then submits the points", async () => {
    const f = fakeChain();
    const w = new ChainWriter(CFG, f.call);
    const r = await w.record(record(), 1000);
    assert.equal(r.status, "done");
    assert.equal(r.tokenId, "tal-1");
    assert.deepEqual(f.calls.map((c) => `${c.contractId} ${c.methodName}`), ["items.gb.testnet mint_talisman", "rite.gb.testnet submit_battle"]);
    assert.deepEqual(f.calls[0]!.args, { to: "alice.testnet", meta: { species: "hushmoth", emotion: "sorrow", omen: "whisper", level: 1, wins: 0 } });
    const result = (f.calls[1]!.args as { result: Record<string, unknown> }).result;
    assert.deepEqual(result.changes, [{ account_id: "alice.testnet", delta: 5 }]);
    assert.equal(result.log_hash, battleLogHash(record()));
    assert.deepEqual(r.txs.map((t) => t.kind), ["mint", "points"]);
    assert.equal(r.txs[0]!.url, "https://testnet.nearblocks.io/txns/tx1");
  });

  test("an exorcism only submits points", async () => {
    const f = fakeChain();
    const r = await new ChainWriter(CFG, f.call).record(record({ outcome: "won", points: 3 }), 1000);
    assert.equal(r.status, "done");
    assert.equal(r.tokenId, undefined);
    assert.deepEqual(f.calls.map((c) => c.methodName), ["submit_battle"]);
  });

  test("a failed call is reported, not thrown, and later battles still go through", async () => {
    const bad = fakeChain("mint_talisman");
    const w = new ChainWriter(CFG, bad.call);
    const r = await w.record(record(), 1000);
    assert.equal(r.status, "failed");
    assert.match(r.error ?? "", /exploded/);
    const ok = await w.record(record({ battleId: "b2", outcome: "won" }), 1000);
    assert.equal(ok.status, "done");
  });

  test("a slow chain answers pending, and the receipt finishes later", async () => {
    const f = fakeChain(undefined, 30);
    const w = new ChainWriter(CFG, f.call);
    const r = await w.record(record());
    assert.equal(r.status, "pending");
    await new Promise((res) => setTimeout(res, 120));
    assert.equal(w.receipt("b1")?.status, "done");
    assert.equal(w.receipt("b1")?.tokenId, "tal-1");
  });

  test("calls run one at a time (they share one key's nonce)", async () => {
    let running = 0;
    let most = 0;
    const call: ContractCall = async () => {
      most = Math.max(most, ++running);
      await new Promise((r) => setTimeout(r, 5));
      running--;
      return { hash: "h", result: "tal-1" };
    };
    const w = new ChainWriter(CFG, call);
    await Promise.all(["a", "b", "c"].map((battleId) => w.record(record({ battleId }), 1000)));
    assert.equal(most, 1);
  });
});

describe("chain config", () => {
  const base = { NEAR_NETWORK: "testnet", NEAR_GAME_ACCOUNT: "gb.testnet", NEAR_ITEMS_CONTRACT: "items.gb.testnet", NEAR_RITE_CONTRACT: "rite.gb.testnet" };

  test("off without accounts, with NEAR_CHAIN=off, or with no key anywhere", () => {
    assert.equal(chainConfigFromEnv({}), null);
    assert.equal(chainConfigFromEnv({ ...base, NEAR_GAME_PRIVATE_KEY: "ed25519:x", NEAR_CHAIN: "off" }), null);
    assert.equal(chainConfigFromEnv({ ...base, NEAR_GAME_ACCOUNT: "nobody-has-this-key-xyz.testnet" }), null);
  });

  test("key from the environment; guest account and RPC default from the game account", () => {
    const c = chainConfigFromEnv({ ...base, NEAR_GAME_PRIVATE_KEY: "ed25519:abc" });
    assert.ok(c);
    assert.equal(c.privateKey, "ed25519:abc");
    assert.equal(c.guestAccount, "guest.gb.testnet");
    assert.equal(c.rpcUrl, "https://test.rpc.fastnear.com");
  });

  test("key from NEAR CLI's legacy keychain when the environment has none", () => {
    const home = mkdtempSync(join(tmpdir(), "gb-chain-"));
    mkdirSync(join(home, ".near-credentials", "testnet"), { recursive: true });
    writeFileSync(join(home, ".near-credentials", "testnet", "gb.testnet.json"), JSON.stringify({ private_key: "ed25519:fromfile" }));
    const saved = { HOME: process.env.HOME, USERPROFILE: process.env.USERPROFILE };
    process.env.HOME = home;
    process.env.USERPROFILE = home;
    try {
      assert.equal(chainConfigFromEnv(base)?.privateKey, "ed25519:fromfile");
    } finally {
      process.env.HOME = saved.HOME;
      process.env.USERPROFILE = saved.USERPROFILE;
    }
  });

  test("account ids and explorer links", () => {
    for (const ok of ["alice.testnet", "relayer.grudgebound.testnet", "a-b_c.testnet", "ab"]) assert.ok(isAccountId(ok), ok);
    for (const bad of ["A.testnet", "a", "a..b", "-a.testnet", "a b", "", 5, "x".repeat(65)]) assert.ok(!isAccountId(bad), String(bad));
    assert.equal(explorerTxUrl("mainnet", "h"), "https://nearblocks.io/txns/h");
  });
});

describe("battle sessions carry the account", () => {
  test("rejects a malformed account and returns a valid one when the battle ends", () => {
    const sessions = new BattleSessions(createWorld(7), { newSeed: () => 1234 });
    assert.equal(sessions.start({ ward: "lantern", account: "Not An Account" }).status, 400);
    const s = sessions.start({ ward: "lantern", account: "alice.testnet" });
    assert.equal(s.status, 200);
    const id = (s.body as { battleId: string }).battleId;
    let last: { outcome: string | null; account?: string } = { outcome: null };
    for (let i = 0; i < 200 && !last.outcome; i++) last = sessions.turn({ battleId: id, action: { kind: "flee" } }).body as typeof last;
    assert.ok(last.outcome);
    assert.equal(last.account, "alice.testnet");
  });
});

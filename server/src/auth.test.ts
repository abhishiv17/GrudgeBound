import { describe, test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { randomBytes } from "node:crypto";
import { AuthService, AuthError, EncryptedAuthStore, SESSION_MS, emailDelivery, playerBody, type AuthStore, type StoredPlayer } from "./auth";
import { playerKeyResolver, playerProvisioner } from "./playerAccounts";
import { ChainWriter, type ChainConfig } from "./chain";
import { createWorld } from "@grudgebound/shared";
import { BattleSessions, type StartAnswer } from "./battles";

function setup(provision?: (p: StoredPlayer, save: () => void) => Promise<void>) {
  let now = 1_000_000;
  const sent: { email: string; code: string }[] = [];
  const store: AuthStore = { data: { players: [], sessions: {} }, save() {} };
  const options = { store, now: () => now, deliver: async (email: string, code: string) => { sent.push({ email, code }); }, provision: provision ?? playerProvisioner(null, null, undefined) };
  const auth = new AuthService(options);
  async function login(email = "binder@example.com") {
    const c = await auth.startEmail(email, "ip");
    const r = await auth.verify(c.challengeId, sent.at(-1)!.code, "ip");
    assert.equal(r.approved, false); if (r.approved) throw new Error("expected browser session");
    return r;
  }
  return { auth, options, store, sent, login, advance: (ms: number) => { now += ms; } };
}
const error = (status: number) => (e: unknown) => e instanceof AuthError && e.status === status;

describe("email sign-in and native handoff", () => {
  test("email proof is required; codes are single-use and profiles persist across sessions", async () => {
    const s = setup();
    const c = await s.auth.startEmail(" Binder@Example.com ", "ip");
    assert.equal(s.sent[0]!.email, "binder@example.com");
    assert.equal(s.store.data.players.length, 0);
    await assert.rejects(s.auth.verify(c.challengeId, "bad", "ip"), error(400));
    const r = await s.auth.verify(c.challengeId, s.sent[0]!.code, "ip");
    assert.equal(r.approved, false); if (r.approved) return;
    await assert.rejects(s.auth.verify(c.challengeId, s.sent[0]!.code, "ip"), error(400));
    assert.equal(s.auth.authenticate(r.token).id, r.player.id);
    const again = await s.login();
    assert.equal(again.player.account, r.player.account);
    assert.equal(s.store.data.players.length, 1);
    assert.ok(!JSON.stringify(s.store.data.sessions).includes(r.token), "only token hashes are stored");
    assert.ok(!("signingKey" in r.player));
  });
  test("five guesses exhaust a challenge, expired and superseded codes cannot sign in", async () => {
    const s = setup();
    const first = await s.auth.startEmail("a@example.com", "ip"); const oldCode = s.sent.at(-1)!.code;
    const second = await s.auth.startEmail("a@example.com", "ip");
    await assert.rejects(s.auth.verify(first.challengeId, oldCode, "ip"), error(400));
    for (let i = 0; i < 5; i++) await assert.rejects(s.auth.verify(second.challengeId, "bad", "ip"), error(400));
    await assert.rejects(s.auth.verify(second.challengeId, s.sent.at(-1)!.code, "ip"), error(400));
    const third = await s.auth.startEmail("a@example.com", "ip"); s.advance(10 * 60_000);
    await assert.rejects(s.auth.verify(third.challengeId, s.sent.at(-1)!.code, "ip"), error(400));
  });
  test("limits email requests and refuses malformed email", async () => {
    const s = setup();
    await assert.rejects(s.auth.startEmail("broken", "ip"), error(400));
    for (let i = 0; i < 3; i++) await s.auth.startEmail("a@example.com", "ip");
    await assert.rejects(s.auth.startEmail("a@example.com", "ip"), error(429));
    s.advance(15 * 60_000); await s.auth.startEmail("a@example.com", "ip");
  });
  test("a delivery failure creates neither a session nor an account", async () => {
    const s = setup(); const auth = new AuthService({ ...s.options, deliver: async () => { throw new Error("mail down"); } });
    await assert.rejects(auth.startEmail("a@example.com", "ip"), error(502));
    assert.equal(s.store.data.players.length, 0);
    assert.deepEqual(s.store.data.sessions, {});
    assert.throws(() => emailDelivery({ NODE_ENV: "production", AUTH_EMAIL_MODE: "console" }), /forbidden/);
  });
  test("device approval cannot be collected using its browser URL and can only be collected once", async () => {
    const s = setup(); const device = s.auth.startDevice("ip", "http://localhost:3000/sign-in");
    const userCode = new URL(device.browserUrl).searchParams.get("device")!;
    assert.ok(!device.browserUrl.includes(device.deviceCode));
    assert.deepEqual(s.auth.pollDevice(device.deviceCode), { pending: true });
    assert.throws(() => s.auth.pollDevice(userCode), error(400));
    const c = await s.auth.startEmail("a@example.com", "ip", userCode);
    const approved = await s.auth.verify(c.challengeId, s.sent.at(-1)!.code, "ip");
    assert.equal(approved.approved, true); assert.ok(!("token" in approved));
    await assert.rejects(s.auth.startEmail("other@example.com", "ip", userCode), error(409));
    s.advance(2000);
    const result = s.auth.pollDevice(device.deviceCode);
    assert.equal(result.pending, false); if (result.pending) return;
    assert.equal(s.auth.authenticate(result.token).email, "a@example.com");
    assert.throws(() => s.auth.pollDevice(device.deviceCode), error(400));
  });
  test("concurrent email approvals cannot replace the player while account creation is pending", async () => {
    let release!: () => void;
    const blocked = new Promise<void>((resolve) => { release = resolve; });
    const s = setup(async (p) => { await blocked; p.account = `p${p.id}.offline`; p.ready = true; });
    const device = s.auth.startDevice("ip", "http://localhost:3000/sign-in");
    const approvalCode = new URL(device.browserUrl).searchParams.get("device")!;
    const first = await s.auth.startEmail("a@example.com", "ip", approvalCode);
    const firstCode = s.sent.at(-1)!.code;
    const second = await s.auth.startEmail("b@example.com", "ip", approvalCode);
    const secondCode = s.sent.at(-1)!.code;
    const pending = s.auth.verify(first.challengeId, firstCode, "ip");
    try {
      await assert.rejects(s.auth.verify(second.challengeId, secondCode, "ip"), error(409));
      await assert.rejects(s.auth.startEmail("c@example.com", "ip", approvalCode), error(409));
      assert.deepEqual(s.auth.pollDevice(device.deviceCode), { pending: true });
    } finally {
      release();
      await pending;
    }
    s.advance(2000);
    const session = s.auth.pollDevice(device.deviceCode);
    assert.equal(session.pending, false);
    if (!session.pending) assert.equal(s.auth.authenticate(session.token).email, "a@example.com");
    assert.equal(s.store.data.players.length, 1);
  });
  test("native approvals expire, polling is bounded, and new approvals cannot replace an approved account", async () => {
    const s = setup(); const device = s.auth.startDevice("ip", "http://localhost:3000/sign-in");
    s.auth.pollDevice(device.deviceCode);
    assert.throws(() => s.auth.pollDevice(device.deviceCode), error(429));
    s.advance(10 * 60_000);
    assert.throws(() => s.auth.pollDevice(device.deviceCode), error(400));
  });
  test("logout and expiry revoke sessions, including after loading the same persistent store", async () => {
    const s = setup(); const r = await s.login();
    const restarted = new AuthService(s.options);
    assert.equal(restarted.authenticate(r.token).id, r.player.id);
    restarted.logout(r.token);
    assert.throws(() => s.auth.authenticate(r.token), error(401));
    const second = await s.login(); s.advance(SESSION_MS);
    assert.throws(() => restarted.authenticate(second.token), error(401));
  });
  test("a provisioning failure retries the same stored player and never returns a session early", async () => {
    let calls = 0;
    const s = setup(async (p) => { p.account ||= `p${p.id}.testnet`; if (++calls === 1) throw new Error("timeout"); p.ready = true; });
    await assert.rejects(s.login(), error(503));
    assert.deepEqual(s.store.data.sessions, {});
    const account = s.store.data.players[0]!.account;
    const retry = await s.login();
    assert.equal(retry.player.account, account); assert.equal(s.store.data.players.length, 1);
  });
  test("server custody requires an explicit testnet option; a saved key survives failed account creation", async () => {
    const p: StoredPlayer = { id: "123", email: "a@example.com", name: "A newcomer", account: "", ready: false };
    const cfg = { network: "testnet", gameAccount: "game.testnet" } as ChainConfig;
    const keys: string[] = [];
    const chain = { provisionAccount: async (_account: string, publicKey: string) => { keys.push(publicKey); if (keys.length === 1) throw new Error("RPC timeout"); } } as unknown as ChainWriter;
    await assert.rejects(playerProvisioner(chain, cfg, undefined)(p, () => {})); assert.equal(p.signingKey, undefined);
    let saved = false;
    const provision = playerProvisioner(chain, cfg, "server");
    await assert.rejects(provision(p, () => { saved = !!p.signingKey; })); assert.ok(saved);
    assert.equal(p.ready, false); const key = p.signingKey;
    await provision(p, () => {}); assert.equal(p.signingKey, key); assert.equal(keys[0], keys[1]); assert.ok(p.ready);
    const store: AuthStore = { data: { players: [p], sessions: {} }, save() {} };
    assert.equal(playerKeyResolver(store)(p.account), key);
    assert.equal(playerKeyResolver(store)("somebody-else.game.testnet"), undefined);
    await assert.rejects(playerProvisioner(chain, { ...cfg, network: "mainnet" }, "server")(p, () => {}));
  });
  test("identity cannot be supplied in a body and another player cannot take a battle turn", async () => {
    const s = setup(); const a = await s.login("a@example.com"); const b = await s.login("b@example.com");
    assert.throws(() => playerBody({ account: b.player.account }, a.player), error(403));
    const battles = new BattleSessions(createWorld(2));
    const started = battles.start(playerBody({ ward: "lantern", by: "someone else" }, a.player));
    assert.equal(started.status, 200);
    const battleId = (started.body as StartAnswer).battleId;
    assert.equal(battles.turn({ battleId, action: { kind: "ward" } }, b.player.account).status, 403);
    const turn = battles.turn({ battleId, action: { kind: "ward" } }, a.player.account);
    assert.equal(turn.status, 200);
  });
  test("account creation and rewards share the same write queue and reject provisioning outside testnet", async () => {
    const order: string[] = [];
    let release!: () => void;
    const blocked = new Promise<void>((r) => { release = r; });
    const writer = new ChainWriter({ network: "testnet", gameAccount: "game.testnet", itemsContract: "items.game.testnet", riteContract: "rite.game.testnet", guestAccount: "guest.game.testnet" }, {
      createAccount: async () => { order.push("create:start"); await blocked; order.push("create:end"); },
      call: async (_contract: string, method: string) => { order.push(method); return { hash: "tx", result: null }; },
    });
    const provision = writer.provisionAccount("p.game.testnet", "public-key");
    const battle = writer.record({ battleId: "b", account: "p.game.testnet", outcome: "won", points: 5, seed: 1, actions: [], wild: { speciesId: "hushmoth", emotion: "dread", omen: "whisper" }, playerCurse: "cinder-grin" }, 1000);
    await new Promise<void>((r) => setImmediate(r));
    assert.deepEqual(order, ["create:start"]);
    release(); await provision; assert.equal((await battle).status, "done");
    assert.deepEqual(order, ["create:start", "create:end", "submit_battle"]);
    await assert.rejects(writer.provisionAccount("other.testnet", "public-key"));
    const mainnet = new ChainWriter({ network: "mainnet", gameAccount: "game.near", itemsContract: "items.game.near", riteContract: "rite.game.near", guestAccount: "guest.game.near" });
    await assert.rejects(mainnet.provisionAccount("p.game.near", "public-key"));
  });
  test("encrypted storage authenticates its contents and keeps plaintext identities and keys out of the file", () => {
    const dir = mkdtempSync(join(tmpdir(), "gb-auth-"));
    try {
      const file = join(dir, "auth.enc"); const key = randomBytes(32);
      const store = new EncryptedAuthStore(file, key);
      store.data.players.push({ id: "p", email: "a@example.com", name: "A newcomer", account: "p.testnet", ready: true, signingKey: "sensitive-example-key" });
      store.save(); const raw = readFileSync(file, "utf8");
      assert.ok(!raw.includes("a@example.com")); assert.ok(!raw.includes("sensitive-example-key"));
      assert.deepEqual(new EncryptedAuthStore(file, key).data, store.data);
      assert.throws(() => new EncryptedAuthStore(file, randomBytes(32)));
    } finally {
      assert.equal(dirname(resolve(dir)), resolve(tmpdir()));
      assert.ok(dir.includes("gb-auth-"));
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

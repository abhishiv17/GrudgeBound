/** Native-session acceptance against the real HTTP server and NEAR testnet.
 * Uses console codes only and a dedicated persistent encrypted test identity.
 * First run funds one new testnet account with 0.05 NEAR; retries reuse it.
 */
import assert from "node:assert/strict";
import { spawn, type ChildProcess } from "node:child_process";
import { createServer } from "node:net";
import { setTimeout as delay } from "node:timers/promises";
import { chainConfigFromEnv } from "./chain";

const cfg = chainConfigFromEnv();
if (!cfg || cfg.network !== "testnet" || process.env.AUTH_KEY_MODE !== "server") throw new Error("Select encrypted server signing on testnet before this check.");
const reservation = createServer();
await new Promise<void>((r) => reservation.listen(0, "127.0.0.1", r));
const port = (reservation.address() as { port: number }).port;
await new Promise<void>((r) => reservation.close(() => r()));
const base = `http://127.0.0.1:${port}`;
const email = "unreal-acceptance@example.invalid";
let child: ChildProcess | undefined, output = "", serverError = "";
async function start() {
  output = ""; serverError = "";
  child = spawn(process.execPath, ["--import", "tsx", "--env-file-if-exists=.env", "server/src/index.ts"], {
    cwd: process.cwd(), windowsHide: true, stdio: ["ignore", "pipe", "pipe"],
    env: { ...process.env, PORT: String(port), HOST: "127.0.0.1", NODE_ENV: "development", AUTH_EMAIL_MODE: "console",
      AUTH_STORE_PATH: "data/native-smoke.enc", AUTH_SIGN_IN_URL: "http://localhost:3000/sign-in", TICK_MS: "60000", NEAR_AI_API_KEY: "" },
  });
  child.stdout!.on("data", (b) => { output += b.toString(); });
  child.stderr!.on("data", (b) => { serverError += b.toString(); });
  for (let i = 0; i < 200; i++) {
    if (child.exitCode !== null) throw new Error(`Acceptance server exited before startup: ${serverError}`);
    if (output.includes("Grudgebound server on")) return;
    await delay(100);
  }
  throw new Error("Acceptance server could not start");
}
async function stop() {
  if (!child || child.exitCode !== null || child.signalCode !== null) return;
  const running = child;
  await new Promise<void>((r) => { running.once("exit", () => r()); running.kill(); });
}
async function request(path: string, token?: string, body?: unknown) {
  const r = await fetch(base + path, {
    method: body === undefined ? "GET" : "POST", signal: AbortSignal.timeout(90_000),
    headers: { "Content-Type": "application/json", ...(token ? { Authorization: `Bearer ${token}` } : {}) },
    ...(body !== undefined ? { body: JSON.stringify(body) } : {}),
  });
  const result = await r.json() as Record<string, any>;
  if (!r.ok) throw new Error(`${path}: ${r.status} ${result.error || "request refused"}`);
  return result;
}
async function receipt(kind: string, id: string, token: string) {
  for (let i = 0; i < 240; i++) {
    const r = await request(`/chain?${kind}=${encodeURIComponent(id)}`, token);
    if (r.status === "failed") throw new Error(`${kind} failed: ${r.error}`);
    if (r.status === "done") {
      for (const tx of r.txs) console.log(`${kind} ${tx.kind}: ${tx.url}`);
      return r;
    }
    await delay(500);
  }
  throw new Error(`Timed out waiting for ${kind}; the encrypted identity is preserved for a retry.`);
}

try {
  await start();
  const device = await request("/auth/device/start", undefined, {});
  const approval = new URL(device.browserUrl).searchParams.get("device");
  const challenge = await request("/auth/email/start", undefined, { email, device: approval });
  const localCode = output.split("\n").reverse().find((line) => line.includes(`[local sign-in] ${email}: `));
  const code = localCode && /: (\d{6})/.exec(localCode)?.[1];
  if (!code) throw new Error("Local verification code was not delivered");
  await request("/auth/email/verify", undefined, { challengeId: challenge.challengeId, code });
  const native = await request("/auth/device/poll", undefined, { deviceCode: device.deviceCode });
  assert.equal(native.pending, false); assert.equal(typeof native.token, "string");
  assert.equal("signingKey" in native.player, false);
  const token = native.token as string;
  const playerAccount = native.player.account as string;
  console.log(`PASS: native approval provisioned/resumed ${playerAccount}; no private key returned.`);
  assert.equal((await request("/auth/session", token)).player.account, playerAccount);
  const initial = await request("/player", token); assert.equal(initial.chain, true);
  let boundToken: string | undefined;
  for (let attempt = 0; attempt < 15 && !boundToken; attempt++) {
    const battle = await request("/battle/start", token, { ward: "lantern" });
    let view = battle.view;
    for (let turn = 0; turn < 200; turn++) {
      const action = view.canUse.salve && view.player.hp < view.player.maxHp * 0.4
        ? { kind: "item", item: "salve" }
        : view.canUse.seal && view.wild.hp < view.wild.maxHp ? { kind: "seal" } : { kind: "attack" };
      const result = await request("/battle/turn", token, { battleId: battle.battleId, action });
      view = result.view;
      if (!result.outcome) continue;
      if (result.chain) {
        const settled = await receipt("battle", battle.battleId, token);
        assert.equal(settled.account, playerAccount);
        if (result.outcome === "bound") boundToken = settled.tokenId;
      }
      break;
    }
  }
  if (!boundToken) throw new Error("No binding after fifteen played encounters; the account is preserved.");
  const afterBind = await request("/player", token);
  assert.ok(afterBind.talismans.some((t: { tokenId: string }) => t.tokenId === boundToken));
  assert.ok(afterBind.points > initial.points);
  console.log("PASS: authenticated battle minted a talisman and earned points for the new identity.");
  const trade = await request("/shop/sell", token, { merchant: "broker", tokenId: boundToken });
  await receipt("shop", trade.jobId, token);
  const afterTrade = await request("/player", token);
  assert.ok(!afterTrade.talismans.some((t: { tokenId: string }) => t.tokenId === boundToken));
  assert.ok(afterTrade.ember > afterBind.ember);
  console.log("PASS: this identity sold its own talisman and received Ember.");
  await stop(); await start();
  assert.equal((await request("/auth/session", token)).player.account, playerAccount);
  const resumed = await request("/player", token);
  assert.equal(resumed.points, afterTrade.points); assert.equal(resumed.ember, afterTrade.ember);
  console.log(`PASS: server restart preserved the native session, account, ${resumed.points} points and ${resumed.ember} Ember.`);
  await request("/auth/logout", token, {});
  assert.equal((await fetch(base + "/auth/session", { headers: { Authorization: `Bearer ${token}` } })).status, 401);
  console.log("PASS: native logout revoked the session. Dedicated encrypted smoke identity retained for reuse.");
} finally { await stop(); }

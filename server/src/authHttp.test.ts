/** Exercise the real server entry point, including route authorization and browser cookies. No chain or mail calls. */
import { test } from "node:test";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { createServer } from "node:net";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { randomBytes } from "node:crypto";
import { setTimeout as delay } from "node:timers/promises";
import { GET as gatewayGet, POST as gatewayPost } from "../../client/app/api/game/[...path]/route";
import { IncomingMessage } from "node:http";
import { Socket } from "node:net";
import { clientIp } from "./authHttp";
import { AuthService } from "./auth";
const json = async <T>(response: Response) => await response.json() as T;

test("proxy sign-in buckets use the right-most IP and ignore a spoofed left-most IP", async () => {
  const req = (header: string) => {
    const socket = new Socket(); Object.defineProperty(socket, "remoteAddress", { value: "127.0.0.1" });
    const request = new IncomingMessage(socket); request.headers["x-forwarded-for"] = header; return request;
  };
  const auth = new AuthService({ store: { data: { players: [], sessions: {} }, save() {} }, deliver: async () => {}, provision: async () => {} });
  for (let i = 0; i < 10; i++) await auth.startEmail(`a${i}@example.com`, clientIp(req("192.0.2.1"), true));
  await assert.rejects(auth.startEmail("spoof@example.com", clientIp(req("203.0.113.99, 192.0.2.1"), true)), { status: 429 });
  await assert.doesNotReject(auth.startEmail("next@example.com", clientIp(req("192.0.2.2"), true)));
  assert.equal(clientIp(req("192.0.2.2"), false), "127.0.0.1");
  assert.equal(clientIp(req("192.0.2.2, invalid"), true), "127.0.0.1");
});

test("HTTP: email login, native approval, authorization, logout and invalid requests", { timeout: 30_000 }, async () => {
  const reservation = createServer(); await new Promise<void>((r) => reservation.listen(0, "127.0.0.1", r));
  const port = (reservation.address() as { port: number }).port;
  await new Promise<void>((r) => reservation.close(() => r()));
  const dir = mkdtempSync(join(tmpdir(), "gb-auth-http-"));
  const child = spawn(process.execPath, ["--import", "tsx", "server/src/index.ts"], {
    cwd: process.cwd(), windowsHide: true, env: { ...process.env, PORT: String(port), HOST: "127.0.0.1", NODE_ENV: "test",
      NEAR_CHAIN: "off", AUTH_EMAIL_MODE: "console", AUTH_SECRET: randomBytes(32).toString("hex"), AUTH_STORE_PATH: join(dir, "auth.enc"), AUTH_SIGN_IN_URL: "http://localhost:3000/sign-in", NEAR_AI_API_KEY: "" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  let output = ""; let errors = "";
  child.stdout.on("data", (b) => { output += b.toString(); }); child.stderr.on("data", (b) => { errors += b.toString(); });
  const base = `http://127.0.0.1:${port}`;
  const previousGateway = process.env.GAME_SERVER_URL;
  process.env.GAME_SERVER_URL = base;
  async function post(path: string, body: unknown, headers: Record<string, string> = {}) {
    return fetch(base + path, { method: "POST", headers: { "Content-Type": "application/json", ...headers }, body: JSON.stringify(body) });
  }
  async function emailCode(email: string, device?: string) {
    const start = await post("/auth/email/start", { email, ...(device ? { device } : {}) }); assert.equal(start.status, 200);
    const { challengeId } = await json<{ challengeId: string }>(start);
    for (let i = 0; i < 50; i++) {
      const line = output.split("\n").reverse().find((l) => l.includes(`[local sign-in] ${email}: `));
      if (line) return { challengeId, code: /: (\d{6})/.exec(line)![1] };
      await delay(20);
    }
    throw new Error("Local email was not delivered");
  }
  try {
    for (let i = 0; i < 150 && !output.includes("Grudgebound server on"); i++) {
      if (child.exitCode !== null) throw new Error(`Server exited before startup: ${errors}`);
      await delay(50);
    }
    assert.ok(output.includes("Grudgebound server on"), "server should start");
    assert.equal((await post("/battle/start", { ward: "lantern" })).status, 401);
    assert.equal((await post("/rules/propose", {})).status, 401);
    assert.equal((await post("/shop/buy", {})).status, 401);
    assert.equal((await post("/vows/swear", {})).status, 401);
    assert.equal((await post("/vows/release", {})).status, 401);
    assert.equal((await fetch(base + "/vows")).status, 401);
    assert.equal((await fetch(base + "/player?account=victim.testnet")).status, 401);
    assert.equal((await post("/auth/email/start", { email: "a@example.com" }, { Origin: "https://evil.example" })).status, 403);
    const code = await emailCode("a@example.com");
    const context = { params: Promise.resolve({ path: ["auth", "email", "verify"] }) };
    const verified = await gatewayPost(new Request("http://localhost:3000/api/game/auth/email/verify", {
      method: "POST", headers: { "Content-Type": "application/json", Origin: "http://localhost:3000" }, body: JSON.stringify(code),
    }), context); assert.equal(verified.status, 200);
    const cookie = verified.headers.get("set-cookie")!;
    assert.match(cookie, /HttpOnly/); assert.match(cookie, /SameSite=Strict/);
    const browserCookie = cookie.split(";")[0]!;
    const identity = await json<{ player: Record<string, unknown> }>(verified); assert.ok(!("token" in identity)); assert.ok(!("signingKey" in identity.player));
    const browser = { Cookie: browserCookie, Origin: "http://localhost:3000" };
    const gatewaySession = await gatewayGet(new Request("http://localhost:3000/api/game/auth/session", { headers: { Cookie: browserCookie } }), { params: Promise.resolve({ path: ["auth", "session"] }) });
    assert.equal(gatewaySession.status, 200);
    assert.equal((await gatewaySession.json() as { player: { email: string } }).player.email, "a@example.com");
    const forged = await gatewayPost(new Request("http://localhost:3000/api/game/auth/logout", { method: "POST", headers: { Origin: "https://evil.example", "Content-Type": "application/json" }, body: "{}" }), { params: Promise.resolve({ path: ["auth", "logout"] }) });
    assert.equal(forged.status, 403);
    assert.equal((await gatewayGet(new Request("http://localhost:3000/api/game/x"), { params: Promise.resolve({ path: ["..", "health"] }) })).status, 404);
    assert.equal((await fetch(base + "/auth/session", { headers: browser })).status, 200);
    assert.equal((await fetch(base + "/player", { headers: browser })).status, 200);
    const shrine = await fetch(base + "/vows", { headers: browser });
    assert.equal(shrine.status, 200);
    const shrineView = await json<{ chain: boolean; templates: { id: string }[]; vows: unknown[] }>(shrine);
    assert.equal(shrineView.chain, false); assert.equal(shrineView.templates.length, 4); assert.deepEqual(shrineView.vows, []);
    assert.equal((await post("/vows/swear", { templateId: "V2", params: {}, days: 1 }, browser)).status, 503);
    assert.equal((await post("/vows/release", { vowId: 0, account: "victim.testnet" }, browser)).status, 403);
    assert.equal((await post("/battle/start", { ward: "lantern", account: "victim.testnet" }, browser)).status, 403);
    const battle = await post("/battle/start", { ward: "lantern" }, browser); assert.equal(battle.status, 200);
    const { battleId } = await json<{ battleId: string }>(battle);

    const deviceStart = await post("/auth/device/start", {}); const device = await json<{ browserUrl: string; deviceCode: string }>(deviceStart);
    const userCode = new URL(device.browserUrl).searchParams.get("device")!;
    const nativeProof = await emailCode("b@example.com", userCode);
    const approval = await post("/auth/email/verify", nativeProof); assert.equal(approval.status, 200);
    assert.equal(approval.headers.get("set-cookie"), null);
    assert.equal((await json<{ approved: boolean }>(approval)).approved, true);
    const poll = await post("/auth/device/poll", { deviceCode: device.deviceCode }); const nativeSession = await json<{ pending: boolean; token: string }>(poll);
    assert.equal(poll.status, 200); assert.equal(nativeSession.pending, false);
    const native = { Authorization: `Bearer ${nativeSession.token}` };
    assert.equal((await post("/battle/turn", { battleId, action: { kind: "ward" } }, native)).status, 403);
    assert.equal((await post("/battle/turn", { battleId, action: { kind: "ward" } }, browser)).status, 200);
    assert.equal((await post("/auth/device/poll", { deviceCode: device.deviceCode })).status, 400);
    assert.equal((await post("/auth/logout", {}, { Cookie: browserCookie })).status, 403, "cookie writes require an origin");
    assert.equal((await post("/auth/logout", {}, browser)).status, 200);
    assert.equal((await fetch(base + "/auth/session", { headers: browser })).status, 401);
    assert.equal((await fetch(base + "/auth/session", { headers: native })).status, 200);
    assert.equal((await post("/auth/logout", {}, native)).status, 200);
    assert.equal((await fetch(base + "/auth/session", { headers: native })).status, 401);
    assert.equal((await fetch(base + "/auth/email/start", { method: "POST", headers: { "Content-Type": "application/json" }, body: "{" })).status, 400);
    assert.equal((await fetch(base + "/auth/email/start", { method: "POST", body: "email=a@example.com" })).status, 415);
  } finally {
    if (previousGateway === undefined) delete process.env.GAME_SERVER_URL; else process.env.GAME_SERVER_URL = previousGateway;
    child.kill(); await new Promise<void>((r) => { if (child.exitCode !== null) r(); else child.once("exit", () => r()); });
    assert.equal(dirname(resolve(dir)), resolve(tmpdir())); // restrict recursive cleanup to this test's temporary directory
    assert.ok(dir.includes("gb-auth-http-"));
    rmSync(dir, { recursive: true, force: true });
  }
});

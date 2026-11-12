import { test, describe, afterEach } from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { randomBytes } from "node:crypto";
import { FileMirror, installMirror, remoteKvFromEnv, upstashKv, type RemoteKv } from "./remoteFiles";
import { EncryptedAuthStore } from "./auth";
import { BattleArchive } from "./battleArchive";

function fakeKv() {
  const data = new Map<string, string>();
  const calls: string[] = [];
  let failures = 0;
  const kv: RemoteKv = {
    async get(k) { calls.push(`get ${k}`); if (failures > 0) { failures--; throw new Error("down"); } return data.get(k) ?? null; },
    async set(k, v) { calls.push(`set ${k}`); await new Promise((r) => setTimeout(r, 1)); if (failures > 0) { failures--; throw new Error("down"); } data.set(k, v); },
    async append(k, v) { calls.push(`append ${k}`); if (failures > 0) { failures--; throw new Error("down"); } data.set(k, (data.get(k) ?? "") + v); },
  };
  return { kv, data, calls, fail: (n: number) => { failures = n; } };
}

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));
async function eventually(check: () => boolean) {
  const deadline = Date.now() + 2000;
  while (!check() && Date.now() < deadline) await wait(10);
  assert.ok(check(), "asynchronous mirror completed before timeout");
}
let dirs: string[] = [];
const tempDir = () => { const d = mkdtempSync(join(tmpdir(), "gb-remote-")); dirs.push(d); return d; };
afterEach(() => { installMirror(null); for (const d of dirs) rmSync(d, { recursive: true, force: true }); dirs = []; });

describe("remote copies of the data files", () => {
  test("restore: the remote copy wins, a local-only file is uploaded, a file nobody has stays absent", async () => {
    const dir = tempDir(), f = fakeKv();
    f.data.set("gb:auth.enc", "remote-auth");
    writeFileSync(join(dir, "auth.enc"), "stale-local");
    writeFileSync(join(dir, "satchels.json"), "local-only");
    await new FileMirror(f.kv).restore([join(dir, "auth.enc"), join(dir, "satchels.json"), join(dir, "agents.enc")]);
    assert.equal(readFileSync(join(dir, "auth.enc"), "utf8"), "remote-auth");
    assert.equal(f.data.get("gb:satchels.json"), "local-only");
    assert.equal(existsSync(join(dir, "agents.enc")), false);
  });

  test("restore throws when the remote store can't be read, so the server never starts empty", async () => {
    const dir = tempDir(), f = fakeKv();
    f.fail(1);
    await assert.rejects(new FileMirror(f.kv).restore([join(dir, "auth.enc")]), /down/);
  });

  test("quick successive changes become one upload of the latest contents; a failed upload is retried", async () => {
    const dir = tempDir(), f = fakeKv(), file = join(dir, "satchels.json");
    const m = new FileMirror(f.kv, { debounceMs: 5, retryMs: 5 });
    writeFileSync(file, "v1"); m.changed(file);
    writeFileSync(file, "v2"); m.changed(file);
    await eventually(() => f.data.get("gb:satchels.json") === "v2");
    assert.equal(f.calls.filter((c) => c === "set gb:satchels.json").length, 1);
    assert.equal(f.data.get("gb:satchels.json"), "v2");
    f.fail(1);
    writeFileSync(file, "v3"); m.changed(file);
    await eventually(() => f.data.get("gb:satchels.json") === "v3");
    assert.equal(f.data.get("gb:satchels.json"), "v3", "stored after the retry");
  });

  test("appends arrive in order even across a failure, and flush waits for them", async () => {
    const dir = tempDir(), f = fakeKv(), file = join(dir, "battles.jsonl");
    const m = new FileMirror(f.kv, { retryMs: 5 });
    f.fail(1);
    m.appended(file, "a\n"); m.appended(file, "b\n"); m.appended(file, "c\n");
    await m.flush(1_000);
    assert.equal(f.data.get("gb:battles.jsonl"), "a\nb\nc\n");
  });

  test("flush uploads a change still waiting for its debounce", async () => {
    const dir = tempDir(), f = fakeKv(), file = join(dir, "agents.enc");
    const m = new FileMirror(f.kv, { debounceMs: 60_000 });
    writeFileSync(file, "pending"); m.changed(file);
    await m.flush(1_000);
    assert.equal(f.data.get("gb:agents.enc"), "pending");
  });

  test("the stores' own writes reach the remote copy, and the auth file stays encrypted there", async () => {
    const dir = tempDir(), f = fakeKv();
    const m = new FileMirror(f.kv, { debounceMs: 1 });
    installMirror(m);
    const store = new EncryptedAuthStore(join(dir, "auth.enc"), randomBytes(32));
    store.data.players.push({ email: "judge@example.com" } as never);
    store.save();
    new BattleArchive(join(dir, "battles.jsonl")).attachChain("b1", "h", "tx", "https://x");
    await m.flush(1_000);
    const remoteAuth = f.data.get("gb:auth.enc")!;
    assert.ok(remoteAuth.length > 0);
    assert.ok(!remoteAuth.includes("judge@example.com"), "only ciphertext leaves the server");
    assert.match(f.data.get("gb:battles.jsonl")!, /"battleId":"b1"/);
  });

  test("configuration: both Upstash variables or neither; https only; REST commands are JSON arrays", async () => {
    assert.equal(remoteKvFromEnv({}), null);
    assert.throws(() => remoteKvFromEnv({ UPSTASH_REDIS_REST_URL: "https://x.upstash.io" }), /both/);
    assert.throws(() => upstashKv("http://x.upstash.io", "t"), /https/);
    const sent: { body: string; auth: string }[] = [];
    const kv = upstashKv("https://x.upstash.io/", "tok", (async (_url: string, init: RequestInit) => {
      sent.push({ body: String(init.body), auth: (init.headers as Record<string, string>).Authorization ?? "" });
      return new Response(JSON.stringify({ result: sent.length === 1 ? "hello" : "OK" }), { status: 200 });
    }) as typeof fetch);
    assert.equal(await kv.get("gb:a"), "hello");
    await kv.set("gb:a", "v");
    assert.deepEqual(sent.map((s) => JSON.parse(s.body)), [["GET", "gb:a"], ["SET", "gb:a", "v"]]);
    assert.equal(sent[0]!.auth, "Bearer tok");
    const failing = upstashKv("https://x.upstash.io", "t", (async () => new Response(JSON.stringify({ error: "WRONGPASS" }), { status: 401 })) as typeof fetch);
    await assert.rejects(failing.get("k"), /WRONGPASS/);
  });
});

/**
 * Keeps the server's few data files (auth.enc, satchels.json, agents.enc, battles.jsonl) in a small remote
 * key-value store, for hosts whose disk is wiped on every restart (Render's free web service).
 *
 * - On start, `restore()` copies each file from the store to its local path (the store wins; a file the store
 *   lacks but the disk has is uploaded). If the store can't be read, the server must not start: starting with
 *   empty files would later overwrite the saved players.
 * - After a store rewrites a file, `fileChanged(file)` uploads the whole file again (debounced, retried).
 * - The battle archive only appends, so `fileAppended(file, text)` sends just the new line (APPEND, in order).
 *
 * The files are uploaded exactly as they are on disk: auth.enc and agents.enc stay AES-256-GCM encrypted with
 * AUTH_SECRET, so the store never sees a key in clear. Upstash Redis's REST API is the only store implemented
 * (UPSTASH_REDIS_REST_URL + UPSTASH_REDIS_REST_TOKEN); without them every hook here is a no-op.
 */
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname } from "node:path";

export interface RemoteKv {
  get(key: string): Promise<string | null>;
  set(key: string, value: string): Promise<void>;
  append(key: string, value: string): Promise<void>;
}

/** Upstash Redis over HTTPS (one JSON command per request). */
export function upstashKv(url: string, token: string, fetchImpl: typeof fetch = fetch): RemoteKv {
  const base = url.replace(/\/+$/, "");
  if (!/^https:\/\//.test(base)) throw new Error("UPSTASH_REDIS_REST_URL must be an https:// URL");
  const command = async (args: string[]): Promise<unknown> => {
    const res = await fetchImpl(base, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
      body: JSON.stringify(args),
      signal: AbortSignal.timeout(15_000),
    });
    const body = (await res.json().catch(() => ({}))) as { result?: unknown; error?: string };
    if (!res.ok || body.error) throw new Error(`remote store ${args[0]} failed (${res.status}${body.error ? `: ${body.error}` : ""})`);
    return body.result;
  };
  return {
    async get(key) { const r = await command(["GET", key]); return typeof r === "string" ? r : null; },
    async set(key, value) { await command(["SET", key, value]); },
    async append(key, value) { await command(["APPEND", key, value]); },
  };
}

export function remoteKvFromEnv(env: NodeJS.ProcessEnv = process.env): RemoteKv | null {
  const url = env.UPSTASH_REDIS_REST_URL?.trim(), token = env.UPSTASH_REDIS_REST_TOKEN?.trim();
  if (!url && !token) return null;
  if (!url || !token) throw new Error("Set both UPSTASH_REDIS_REST_URL and UPSTASH_REDIS_REST_TOKEN, or neither");
  return upstashKv(url, token);
}

export class FileMirror {
  private readonly pending = new Map<string, { timer?: ReturnType<typeof setTimeout>; running?: Promise<void>; dirty: boolean }>();
  private readonly appends = new Map<string, Promise<void>>();

  constructor(
    private readonly kv: RemoteKv,
    private readonly options: { prefix?: string; debounceMs?: number; retryMs?: number; log?: (msg: string) => void } = {},
  ) {}

  key(file: string): string { return (this.options.prefix ?? "gb:") + basename(file); }

  /** Before any store opens its file. Throws if the remote store can't be read. */
  async restore(files: string[]): Promise<void> {
    for (const file of files) {
      const remote = await this.kv.get(this.key(file));
      if (remote !== null) {
        mkdirSync(dirname(file), { recursive: true });
        writeFileSync(file, remote, { mode: 0o600 });
      } else if (existsSync(file)) {
        await this.kv.set(this.key(file), readFileSync(file, "utf8"));
      }
    }
  }

  /** A store rewrote `file`: upload its current contents soon (several quick writes become one upload). */
  changed(file: string): void {
    const p = this.pending.get(file) ?? { dirty: false };
    this.pending.set(file, p);
    p.dirty = true;
    if (p.timer || p.running) return;
    p.timer = setTimeout(() => this.upload(file), this.options.debounceMs ?? 500);
  }

  private upload(file: string): void {
    const p = this.pending.get(file)!;
    p.timer = undefined;
    p.dirty = false;
    p.running = this.kv.set(this.key(file), readFileSync(file, "utf8")).then(
      () => {},
      (e: Error) => { p.dirty = true; this.options.log?.(`Remote save of ${basename(file)} failed, retrying: ${e.message}`); },
    ).finally(() => {
      p.running = undefined;
      if (p.dirty) p.timer = setTimeout(() => this.upload(file), this.options.retryMs ?? 5_000);
    });
  }

  /** The archive appended `text` to `file`: send it in order, retrying until it is stored. */
  appended(file: string, text: string): void {
    const key = this.key(file);
    const send = async (): Promise<void> => {
      for (;;) {
        try { return await this.kv.append(key, text); } catch (e) {
          this.options.log?.(`Remote append to ${basename(file)} failed, retrying: ${(e as Error).message}`);
          await new Promise((r) => setTimeout(r, this.options.retryMs ?? 5_000));
        }
      }
    };
    this.appends.set(file, (this.appends.get(file) ?? Promise.resolve()).then(send));
  }

  /** Upload everything still waiting (on shutdown). Resolves after `timeoutMs` at the latest. */
  async flush(timeoutMs = 8_000): Promise<void> {
    for (const [file, p] of this.pending) {
      if (p.timer) { clearTimeout(p.timer); p.timer = undefined; }
      if (p.dirty && !p.running) this.upload(file);
    }
    const work = Promise.all([...[...this.pending.values()].map((p) => p.running), ...this.appends.values()]);
    await Promise.race([work, new Promise((r) => setTimeout(r, timeoutMs).unref?.())]);
  }
}

let mirror: FileMirror | null = null;
/** index.ts installs the mirror after a successful restore; until then (and in tests) the hooks do nothing. */
export function installMirror(m: FileMirror | null): void { mirror = m; }
export function fileChanged(file: string): void { mirror?.changed(file); }
export function fileAppended(file: string, text: string): void { mirror?.appended(file, text); }

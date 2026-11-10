/** Email-code authentication and single-use browser approval for the native client (P3-C1).
 * Tokens are opaque; only their hashes are saved. The encrypted store is for one server process.
 */
import { createCipheriv, createDecipheriv, createHash, createHmac, randomBytes, randomInt, timingSafeEqual } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { fileChanged } from "./remoteFiles";
import { dirname, resolve } from "node:path";

export const SESSION_MS = 7 * 24 * 60 * 60_000;
const CODE_MS = 10 * 60_000;
const secret = () => randomBytes(32).toString("base64url");
export const tokenHash = (s: string) => createHash("sha256").update(s).digest("hex");
export class AuthError extends Error {
  constructor(readonly status: number, message: string) { super(message); }
}
export interface AuthPlayer { id: string; email: string; name: string; account: string; ready: boolean }
export interface StoredPlayer extends AuthPlayer { signingKey?: string }
interface StoredSession { playerId: string; expiresAt: number }
export interface AuthData { players: StoredPlayer[]; sessions: Record<string, StoredSession> }
export interface AuthStore { data: AuthData; save(): void }

/** AES-256-GCM authenticates the complete file, including player/account mappings. */
export class EncryptedAuthStore implements AuthStore {
  data: AuthData = { players: [], sessions: {} };
  constructor(private readonly file: string, private readonly key: Buffer) {
    if (key.length !== 32) throw new Error("AUTH_SECRET must be 64 hexadecimal characters");
    if (existsSync(file)) {
      const packed = Buffer.from(readFileSync(file, "utf8"), "base64");
      const decipher = createDecipheriv("aes-256-gcm", key, packed.subarray(0, 12));
      decipher.setAuthTag(packed.subarray(12, 28));
      this.data = JSON.parse(Buffer.concat([decipher.update(packed.subarray(28)), decipher.final()]).toString("utf8"));
    }
  }
  save() {
    mkdirSync(dirname(this.file), { recursive: true });
    const iv = randomBytes(12);
    const cipher = createCipheriv("aes-256-gcm", this.key, iv);
    const body = Buffer.concat([cipher.update(JSON.stringify(this.data), "utf8"), cipher.final()]);
    const tmp = `${this.file}.tmp`;
    writeFileSync(tmp, Buffer.concat([iv, cipher.getAuthTag(), body]).toString("base64"), { mode: 0o600 });
    renameSync(tmp, this.file);
    fileChanged(this.file);
  }
}

export function authStoreFromEnv(env: NodeJS.ProcessEnv = process.env): EncryptedAuthStore {
  return new EncryptedAuthStore(resolve(env.AUTH_STORE_PATH || "data/auth.enc"), authKeyFromEnv(env));
}

/** The auth store's 32-byte key (AUTH_SECRET, or the local development key file). Other encrypted stores reuse it. */
export function authKeyFromEnv(env: NodeJS.ProcessEnv = process.env): Buffer {
  const file = resolve(env.AUTH_STORE_PATH || "data/auth.enc");
  let keyHex = env.AUTH_SECRET;
  if (keyHex && !/^[a-fA-F0-9]{64}$/.test(keyHex)) throw new Error("AUTH_SECRET must be 64 hexadecimal characters");
  if (!keyHex) {
    if (env.NODE_ENV === "production") throw new Error("AUTH_SECRET is required in production");
    const keyFile = `${file}.key`;
    if (!existsSync(keyFile)) {
      mkdirSync(dirname(keyFile), { recursive: true });
      writeFileSync(keyFile, randomBytes(32).toString("hex"), { mode: 0o600, flag: "wx" });
    }
    keyHex = readFileSync(keyFile, "utf8").trim();
    if (!/^[a-fA-F0-9]{64}$/.test(keyHex)) throw new Error("Invalid local auth store key");
  }
  return Buffer.from(keyHex, "hex");
}

export type DeliverCode = (email: string, code: string) => Promise<void>;
export function emailDelivery(env: NodeJS.ProcessEnv = process.env): DeliverCode {
  if (env.AUTH_EMAIL_MODE === "console") {
    if (env.NODE_ENV === "production") throw new Error("Console email is forbidden in production");
    return async (email, code) => { console.log(`[local sign-in] ${email}: ${code}`); };
  }
  return async (email, code) => {
    if (!env.RESEND_API_KEY || !env.AUTH_EMAIL_FROM) throw new AuthError(503, "Email sign-in is not configured yet.");
    const response = await fetch("https://api.resend.com/emails", {
      method: "POST", signal: AbortSignal.timeout(10_000),
      headers: { Authorization: `Bearer ${env.RESEND_API_KEY}`, "Content-Type": "application/json" },
      body: JSON.stringify({ from: env.AUTH_EMAIL_FROM, to: [email], subject: "Your Hollowmere sign-in code",
        text: `Your Grudgebound sign-in code is ${code}. It expires in 10 minutes. Only enter it in the sign-in window you opened. If you didn't request it, ignore this email.` }),
    });
    if (!response.ok) throw new AuthError(502, "The sign-in email could not be sent. Try again later.");
  };
}

interface CodeChallenge { email: string; digest: string; salt: string; expiresAt: number; tries: number; device?: string }
interface Device { deviceHash: string; expiresAt: number; playerId?: string; approving?: boolean; lastPoll: number }
export interface AuthOptions {
  store: AuthStore; deliver: DeliverCode;
  /** Must save any generated key before sending the account-creation transaction. */
  provision: (player: StoredPlayer, save: () => void) => Promise<void>;
  isPlayerReady?: (player: StoredPlayer) => boolean;
  now?: () => number;
}

export class AuthService {
  private readonly codes = new Map<string, CodeChallenge>();
  private readonly devices = new Map<string, Device>();
  private readonly limits = new Map<string, { count: number; until: number }>();
  private readonly provisioning = new Map<string, Promise<void>>();
  private readonly now: () => number;
  constructor(private readonly opts: AuthOptions) { this.now = opts.now ?? Date.now; }

  private prune() {
    const now = this.now();
    for (const [k, v] of this.codes) if (v.expiresAt <= now) this.codes.delete(k);
    for (const [k, v] of this.devices) if (v.expiresAt <= now) this.devices.delete(k);
    for (const [k, v] of this.limits) if (v.until <= now) this.limits.delete(k);
    for (const [k, v] of Object.entries(this.opts.store.data.sessions)) if (v.expiresAt <= now) delete this.opts.store.data.sessions[k];
  }
  private limit(key: string, max: number, window: number) {
    this.prune();
    const rate = this.limits.get(key) ?? { count: 0, until: this.now() + window };
    if (rate.count >= max) throw new AuthError(429, "Too many sign-in attempts. Please wait and try again.");
    rate.count++; this.limits.set(key, rate);
  }
  private publicPlayer(p: StoredPlayer): AuthPlayer {
    return { id: p.id, email: p.email, name: p.name, account: p.account, ready: p.ready };
  }
  private device(code: unknown): Device {
    const d = typeof code === "string" ? this.devices.get(code) : undefined;
    if (!d || d.expiresAt <= this.now()) throw new AuthError(400, "The game sign-in has expired. Start again in the game.");
    return d;
  }
  startDevice(ip: string, signInUrl: string) {
    this.limit(`device:${ip}`, 15, 15 * 60_000);
    const userCode = secret();
    const deviceCode = secret();
    this.devices.set(userCode, { deviceHash: tokenHash(deviceCode), expiresAt: this.now() + CODE_MS, lastPoll: 0 });
    const url = new URL(signInUrl); url.searchParams.set("device", userCode);
    return { deviceCode, browserUrl: url.toString(), expiresIn: CODE_MS / 1000, interval: 2 };
  }
  pollDevice(code: unknown) {
    this.prune();
    if (typeof code !== "string" || code.length > 100) throw new AuthError(400, "Invalid game sign-in.");
    const hash = tokenHash(code);
    const entry = [...this.devices.entries()].find(([, d]) => d.deviceHash === hash);
    if (!entry) throw new AuthError(400, "The game sign-in has expired. Start again in the game.");
    const [id, d] = entry;
    if (d.lastPoll && this.now() - d.lastPoll < 1500) throw new AuthError(429, "Wait before checking sign-in again.");
    d.lastPoll = this.now();
    if (!d.playerId) return { pending: true as const };
    const p = this.opts.store.data.players.find((p) => p.id === d.playerId)!;
    const session = this.issue(p);
    this.devices.delete(id);
    return { pending: false as const, ...session };
  }
  async startEmail(rawEmail: unknown, ip: string, device?: unknown) {
    this.limit(`send-ip:${ip}`, 10, 15 * 60_000);
    if (typeof rawEmail !== "string") throw new AuthError(400, "Enter a valid email address.");
    const email = rawEmail.trim().toLowerCase();
    if (email.length > 254 || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) throw new AuthError(400, "Enter a valid email address.");
    if (device !== undefined) {
      const d = this.device(device);
      if (d.playerId || d.approving) throw new AuthError(409, "This game sign-in is already being approved.");
    }
    this.limit(`send-email:${email}`, 3, 15 * 60_000);
    const challengeId = secret();
    const code = String(randomInt(0, 1_000_000)).padStart(6, "0");
    const salt = secret();
    const digest = createHmac("sha256", salt).update(code).digest("hex");
    try { await this.opts.deliver(email, code); }
    catch (e) { throw e instanceof AuthError ? e : new AuthError(502, "The sign-in email could not be sent. Try again later."); }
    // Sending a new code invalidates old codes for this email.
    for (const [id, c] of this.codes) if (c.email === email) this.codes.delete(id);
    this.codes.set(challengeId, { email, digest, salt, expiresAt: this.now() + CODE_MS, tries: 0, device: device as string | undefined });
    return { challengeId, expiresIn: CODE_MS / 1000 };
  }
  async verify(challengeId: unknown, code: unknown, ip: string) {
    this.limit(`verify:${ip}`, 30, 15 * 60_000);
    const c = typeof challengeId === "string" ? this.codes.get(challengeId) : undefined;
    if (!c || c.expiresAt <= this.now() || c.tries >= 5) throw new AuthError(400, "This code has expired. Request a new one.");
    c.tries++;
    const supplied = typeof code === "string" && /^\d{6}$/.test(code) ? code : "invalid";
    const digest = createHmac("sha256", c.salt).update(supplied).digest();
    if (!timingSafeEqual(digest, Buffer.from(c.digest, "hex"))) throw new AuthError(400, "That code isn't correct. Try again.");
    const device = c.device ? this.device(c.device) : undefined;
    if (device?.playerId || device?.approving) throw new AuthError(409, "This game sign-in is already being approved.");
    this.codes.delete(challengeId as string); // consumed before any asynchronous provisioning
    // Reserve this handoff before provisioning yields: another verified email must not replace its player.
    if (device) device.approving = true;
    try {
      let player = this.opts.store.data.players.find((p) => p.email === c.email);
      if (!player) {
        const id = randomBytes(12).toString("hex");
        player = { id, email: c.email, name: "A newcomer", account: "", ready: false };
        this.opts.store.data.players.push(player); this.opts.store.save();
      }
      const p = player;
      let pending = this.provisioning.get(p.id);
      if (!pending) {
        pending = this.opts.provision(p, () => this.opts.store.save());
        this.provisioning.set(p.id, pending);
      }
      try { await pending; }
      catch { throw new AuthError(503, "Your place in Hollowmere couldn't be prepared. Sign in again to retry."); }
      finally { this.provisioning.delete(p.id); }
      this.opts.store.save();
      if (device) {
        if (device.expiresAt <= this.now()) throw new AuthError(400, "The game sign-in expired. Start again in the game.");
        device.playerId = p.id;
        return { approved: true as const, player: this.publicPlayer(p) };
      }
      return { approved: false as const, ...this.issue(p) };
    } finally {
      if (device) device.approving = false;
    }
  }
  private issue(p: StoredPlayer) {
    if (!p.ready || this.opts.isPlayerReady?.(p) === false) throw new AuthError(503, "Please finish signing in before entering Hollowmere.");
    this.prune();
    const token = secret(); const expiresAt = this.now() + SESSION_MS;
    // Bound the stored sessions per player, keeping the newest devices.
    const old = Object.entries(this.opts.store.data.sessions).filter(([, s]) => s.playerId === p.id).sort((a, b) => a[1].expiresAt - b[1].expiresAt);
    while (old.length >= 10) delete this.opts.store.data.sessions[old.shift()![0]];
    this.opts.store.data.sessions[tokenHash(token)] = { playerId: p.id, expiresAt };
    this.opts.store.save();
    return { token, expiresAt, player: this.publicPlayer(p) };
  }
  authenticate(token: string | undefined): AuthPlayer {
    if (!token || token.length > 100) throw new AuthError(401, "Sign in to enter Hollowmere.");
    const s = this.opts.store.data.sessions[tokenHash(token)];
    if (!s || s.expiresAt <= this.now()) throw new AuthError(401, "Your sign-in expired. Please sign in again.");
    const p = this.opts.store.data.players.find((p) => p.id === s.playerId);
    if (!p?.ready || this.opts.isPlayerReady?.(p) === false) throw new AuthError(401, "Please finish signing in.");
    return this.publicPlayer(p);
  }
  logout(token: string | undefined) {
    if (token) { delete this.opts.store.data.sessions[tokenHash(token)]; this.opts.store.save(); }
  }
}

/** Reject account impersonation instead of silently accepting an account supplied by the client. */
export function playerBody(body: unknown, player: AuthPlayer): Record<string, unknown> {
  if (!body || typeof body !== "object" || Array.isArray(body)) throw new AuthError(400, "Expected a JSON object.");
  const b = body as Record<string, unknown>;
  if (b.account !== undefined && b.account !== player.account) throw new AuthError(403, "You can only act as your signed-in player.");
  return { ...b, account: player.account, by: player.name };
}

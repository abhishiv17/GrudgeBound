/**
 * Game server (Day 1 skeleton).
 *
 * Runs the shared tick engine in real time and exposes the world over HTTP:
 *   GET /health          -> { ok: true }
 *   GET /state           -> world summary (Grudge, merchants, leaderboard, rules)
 *   GET /events?since=N  -> events after tick N
 *   POST /battle/start   -> { ward, playerCurse?, by?, account? }: the server picks the curse and the seed, and returns the battle view
 *   POST /battle/turn    -> { battleId, action }: the server resolves the turn (see battles.ts; the Unreal and web clients)
 *                           Every battle runs here, so a client can't retry seeds or claim a rarer curse.
 *   POST /rules/propose  -> { account, templateId, params }: Kessa's verdict, then the rule goes through the rite
 *                           contract (relayed for the player, resolved by the game master); see rules.ts
 *   POST /rules/repeal   -> { account, ruleId }: strikes a rule in force for twice its cost (relayed; ruleId = rite id)
 *   GET /chain?rule=ID   -> that proposal's (or repeal's) on-chain receipt
 *   GET /rules?account=A -> rule templates (with display labels), rules in force, the cost and A's points
 *   GET /chain?battle=ID -> that battle's on-chain receipt (talisman id, tx links); without ?battle: the contracts in use
 *                           Both battle routes answer with `chain` when a binding or exorcism is written to NEAR (chain.ts)
 *   GET /curses          -> every curse's id, name, emotion, omen, home Ward and signature move (no stats)
 *   POST /shop/buy, /shop/sell -> a player trades talismans for Ember on chain (see shop.ts); GET /chain?shop=ID
 *   GET /shop?merchant=broker&account=A -> also A's Ember on chain and what the merchant pays per talisman
 *   GET /shop?merchant=broker -> that merchant's prices, each with a one-line reason in the merchant's voice
 *                           (NEAR AI Cloud if .env has a key, else the plain formula reason; cached per tick)
 *   POST /tick           -> advance one tick now (dev only)
 *
 * Sign-in: /auth/email/start, /auth/email/verify, /auth/session, /auth/logout.
 * Unreal: /auth/device/start and /auth/device/poll. See docs/SIGN_IN.md for setup and key custody.
 * Next: agents with their own NEAR accounts, vows.
 */
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";
import {
  createWorld, createRng, step, summarize, log,
  DEFAULT_REAL_MS_PER_TICK, DEMO_SEED,
  DEFAULT_PLAYER_CURSE,
} from "@grudgebound/shared";
import { llmConfigFromEnv, priceTooltip, type Tooltip } from "@grudgebound/agents";
import { BattleSessions, curseCatalog, type TurnAnswer } from "./battles";
import { ChainWriter, chainConfigFromEnv, custodialKey, type BattleRecord } from "./chain";
import { ChainRules, rulesOverview } from "./rules";
import { playerView } from "./player";
import { describeChainEvent } from "./chainEvents";
import { BattleArchive } from "./battleArchive";
import { Shop } from "./shop";
import { AuthError, AuthService, authStoreFromEnv, emailDelivery, playerBody } from "./auth";
import { authRoute, sessionToken } from "./authHttp";
import { playerKeyResolver, playerProvisioner } from "./playerAccounts";
import { Vows } from "./vows";

const PORT = Number(process.env.PORT ?? 8787);
const TICK_MS = Number(process.env.TICK_MS ?? DEFAULT_REAL_MS_PER_TICK);
const SEED = Number(process.env.WORLD_SEED ?? DEMO_SEED);
const DEV = process.env.NODE_ENV !== "production";
const RULE_SYNC_MS = 60_000;
const SIGN_IN_URL = process.env.AUTH_SIGN_IN_URL || "http://localhost:3000/sign-in";
const signInUrl = new URL(SIGN_IN_URL);
if (!DEV && signInUrl.protocol !== "https:") throw new Error("AUTH_SIGN_IN_URL must use HTTPS in production");
if (!["http:", "https:"].includes(signInUrl.protocol) || signInUrl.username || signInUrl.password ||
    (signInUrl.protocol === "http:" && !["localhost", "127.0.0.1", "[::1]"].includes(signInUrl.hostname))) throw new Error("AUTH_SIGN_IN_URL must use HTTPS or loopback HTTP");
const HOST = process.env.HOST || (DEV ? "127.0.0.1" : "0.0.0.0");
if (process.env.AUTH_EMAIL_MODE === "console" && !["127.0.0.1", "::1", "localhost"].includes(HOST)) throw new Error("Console email requires a loopback-only server");
const authStore = authStoreFromEnv();

const world = createWorld(SEED);
const rng = createRng(SEED);

setInterval(() => step(world, rng), TICK_MS);

const CORS = {
  "Access-Control-Allow-Origin": signInUrl.origin,
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
  "Access-Control-Allow-Credentials": "true",
  "Vary": "Origin",
};

function send(res: ServerResponse, status: number, body: unknown) {
  res.writeHead(status, { "Content-Type": "application/json", "Cache-Control": "no-store", ...CORS });
  res.end(JSON.stringify(body));
}

const MAX_BODY_BYTES = 16 * 1024;
/** Seeds already counted, so the same battle cannot be reported twice (shared by both battle routes). */
const battles = new BattleSessions(world);

function readJson(req: IncomingMessage): Promise<unknown> {
  return new Promise((resolve, reject) => {
    let size = 0;
    const chunks: Buffer[] = [];
    req.on("data", (c: Buffer) => {
      size += c.length;
      if (size > MAX_BODY_BYTES) {
        reject(new Error("body too large"));
        req.destroy();
      } else chunks.push(c);
    });
    req.on("end", () => {
      try {
        resolve(JSON.parse(Buffer.concat(chunks).toString("utf8")));
      } catch {
        reject(new Error("body is not JSON"));
      }
    });
    req.on("error", reject);
  });
}

const chainCfg = chainConfigFromEnv();
if (chainCfg && process.env.AUTH_KEY_MODE === "server") chainCfg.playerKey = playerKeyResolver(authStore);
const chain = chainCfg ? new ChainWriter(chainCfg) : null;
const auth = new AuthService({ store: authStore, deliver: emailDelivery(), provision: playerProvisioner(chain, chainCfg, process.env.AUTH_KEY_MODE),
  isPlayerReady: (p) => !chainCfg ? p.account.endsWith(".offline") : process.env.AUTH_KEY_MODE === "server" && !!p.signingKey && p.account.endsWith(`.${chainCfg.gameAccount}`),
});
/** Replay records of every battle the Rite paid for (npm run replay <seed>). */
const archive = new BattleArchive();
// What the contracts logged (NEP-297) joins the town's event feed, with a link to the transaction.
chain?.onEvents((events) => {
  for (const e of events) {
    const d = e.data[0];
    if (e.event === "battle" && typeof d?.battle_id === "string" && typeof d.log_hash === "string") {
      archive.attachChain(d.battle_id, d.log_hash, e.txHash, e.url);
    }
    const said = describeChainEvent(e, chainCfg!.gameAccount);
    if (said) log(world, { ...said, url: e.url });
  }
});
const canSignFor = (a: string) => !!chainCfg && (chainCfg.playerKey?.(a) ?? custodialKey(chainCfg, a)) !== undefined;
const chainRules = chain && chainCfg ? new ChainRules(world, chain, canSignFor) : null;
const playerShop = chain && chainCfg ? new Shop(world, chain, chainCfg.gameAccount, canSignFor) : null;
const vows = new Vows(world, chain, canSignFor, TICK_MS);
let vowsSweeping = false;
function sweepVows() {
  if (vowsSweeping || !chain) return;
  vowsSweeping = true;
  void vows.sweep(authStore.data.players.filter((p) => p.ready).map((p) => p.account))
    .catch(() => console.warn("Vows: settlement will retry when the Rite is reachable."))
    .finally(() => { vowsSweeping = false; });
}

/** After a battle route answers 200 with a binding or exorcism: write it on chain and add the receipt. */
async function withChain<T extends object>(body: T, record: BattleRecord | null): Promise<T> {
  if (!chain || !record || record.points <= 0) return body;
  return { ...body, chain: await chain.record(record) };
}

/** A binding or exorcism's replay record (also when the server runs off-chain: the archive keeps it). */
function recordFromTurn(t: TurnAnswer): BattleRecord | null {
  if ((t.outcome !== "bound" && t.outcome !== "won") || t.seed === undefined || !t.actions) return null;
  const w = t.view.wild;
  return {
    battleId: t.battleId, account: t.account ?? chain?.cfg.guestAccount ?? "guest", outcome: t.outcome, points: t.points ?? 0,
    seed: t.seed, actions: t.actions, wild: { speciesId: w.speciesId, emotion: w.emotion, omen: w.omen },
    playerCurse: t.view.player.speciesId,
  };
}

const llm = llmConfigFromEnv();
type ShopLine = Tooltip & { id: string; name: string; stock: number };
/** Shop answers per merchant, recomputed once per tick (or after a trade) so prices stay live. */
const shopCache = new Map<string, { tick: number; body: { items: ShopLine[] } & Record<string, unknown> }>();
/** The merchant's last AI-written line per item, with the price it was written for. */
const aiLines = new Map<string, { price: number; text: string }>();
/** Merchants whose AI lines are being written right now. */
const writing = new Set<string>();

/**
 * GET /shop never waits for the AI (four NEAR AI calls took ~8 s, past the Unreal client's timeout): it answers
 * at once with live prices and the formula's reason, or the merchant's earlier AI line when the price is unchanged,
 * and has the AI write fresh lines in the background, all items in parallel; the next request shows them.
 */
async function shop(merchantId: string): Promise<{ status: number; body: unknown }> {
  const m = world.merchants.find((x) => x.id === merchantId);
  if (!m) return { status: 404, body: { error: `no merchant "${merchantId}"`, merchants: world.merchants.map((x) => x.id) } };
  const hit = shopCache.get(m.id);
  if (hit && hit.tick === world.tick) return { status: 200, body: hit.body };
  const items: ShopLine[] = [];
  for (const item of m.items) {
    const t = await priceTooltip(world, m, item.id, null);
    const said = aiLines.get(`${m.id}:${item.id}`);
    items.push({ id: item.id, name: item.name, stock: item.stock, ...t, ...(said && said.price === t.price ? { text: said.text, source: "llm" as const } : {}) });
  }
  const body = { merchant: { id: m.id, name: m.name, ward: m.ward, closed: m.closed }, tick: world.tick, items };
  shopCache.set(m.id, { tick: world.tick, body });
  if (llm && !writing.has(m.id) && items.some((i) => i.source !== "llm")) {
    writing.add(m.id);
    const tick = world.tick;
    void Promise.all(m.items.map((item) => priceTooltip(world, m, item.id, llm).then((t) => ({ id: item.id, t }))))
      .then((lines) => {
        for (const { id, t } of lines) if (t.source === "llm") aiLines.set(`${m.id}:${id}`, { price: t.price, text: t.text });
        const cached = shopCache.get(m.id);
        if (cached && cached.tick === tick) shopCache.delete(m.id); // the next request picks the new lines up
      })
      .catch((e: Error) => console.warn(`merchant lines for ${m.id}: ${e.message}`))
      .finally(() => writing.delete(m.id));
  }
  return { status: 200, body };
}

async function handle(req: IncomingMessage, res: ServerResponse) {
  const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);
  if (req.headers.origin && req.headers.origin !== signInUrl.origin) throw new AuthError(403, "Request origin is not allowed.");
  if (req.method === "POST" && !(req.headers["content-type"] ?? "").startsWith("application/json")) throw new AuthError(415, "Expected application/json.");
  if (req.method === "POST" && req.headers.cookie && !req.headers.origin && !req.headers.authorization) throw new AuthError(403, "Browser requests must include their origin.");
  if (req.method === "OPTIONS") {
    res.writeHead(204, CORS);
    return res.end();
  }
  if (url.pathname.startsWith("/auth/")) {
    return authRoute(auth, req, res, url.pathname, () => readJson(req), (status, body) => send(res, status, body), SIGN_IN_URL, !DEV);
  }
  if (req.method === "GET" && url.pathname === "/vows") {
    const player = auth.authenticate(sessionToken(req));
    sweepVows();
    return send(res, 200, await vows.overview(player.account));
  }
  if (req.method === "POST" && (url.pathname === "/vows/swear" || url.pathname === "/vows/release")) {
    const player = auth.authenticate(sessionToken(req));
    const body = playerBody(await readJson(req), player);
    const result = url.pathname === "/vows/swear" ? await vows.swear(player.account, body) : await vows.release(player.account, body.vowId);
    // The Shrine is outside encounters. Old encounters (including a slipped-away one) cannot resume under changed vows.
    battles.abandonAccount(player.account);
    return send(res, 200, result);
  }
  if (req.method === "POST" && (url.pathname === "/battle/start" || url.pathname === "/battle/turn")) {
    const player = auth.authenticate(sessionToken(req));
    const body = playerBody(await readJson(req), player);
    const activeVows = await vows.snapshot(player.account);
    const vowGuard: import("./battles").BattleGuard = (context, action) => vows.battleGuard(activeVows, context, action);
    if (url.pathname === "/battle/start") {
      if (chain && typeof body.playerCurse === "string" && body.playerCurse !== DEFAULT_PLAYER_CURSE) {
        const owned = await chain.talismansOf(player.account);
        if (!owned.some((t) => t.meta.species === body.playerCurse)) throw new AuthError(403, "Choose a partner from your own talismans.");
      }
      const r = battles.start(body, vowGuard);
      return send(res, r.status, r.body);
    }
    const r = battles.turn(body, player.account, vowGuard);
    if (r.status !== 200) return send(res, r.status, r.body);
    const t = r.body as TurnAnswer;
    const record = recordFromTurn(t);
    if (record) {
      try { archive.add(record, t.ward ?? "lantern"); } catch (e) { console.warn(`battle archive: ${(e as Error).message}`); }
    }
    return send(res, 200, await withChain(t, record));
  }
  if (req.method === "GET" && url.pathname === "/shop") {
    const merchantId = url.searchParams.get("merchant") ?? "broker";
    const r = await shop(merchantId);
    if (r.status !== 200 || !playerShop) return send(res, r.status, r.body);
    const token = sessionToken(req);
    const account = token ? auth.authenticate(token).account : url.searchParams.get("account");
    const extras = await playerShop.extras(merchantId, account);
    return send(res, 200, { ...(r.body as object), ...extras });
  }
  if (req.method === "POST" && (url.pathname === "/shop/buy" || url.pathname === "/shop/sell")) {
    const player = auth.authenticate(sessionToken(req));
    if (!playerShop) return send(res, 503, { error: "trades are settled on chain, and this server runs off-chain" });
    const body = playerBody(await readJson(req), player);
    const merchant = world.merchants.find((m) => m.id === body.merchant);
    if (merchant) vows.assertTrade(await vows.snapshot(player.account), merchant.ward, url.pathname === "/shop/sell" ? body.tokenId : undefined);
    const r = await (url.pathname === "/shop/buy" ? playerShop.buy(body) : playerShop.sell(body));
    // Stock and demand just changed: the next GET /shop recomputes prices (and Kessa's tooltip lines).
    const m = body.merchant;
    if (r.status === 200 && typeof m === "string") shopCache.delete(m);
    send(res, r.status, r.body);
    return;
  }
  if (req.method === "POST" && (url.pathname === "/rules/propose" || url.pathname === "/rules/repeal")) {
    const player = auth.authenticate(sessionToken(req));
    if (!chainRules) return send(res, 503, { error: "rules go through the rite contract, and this server runs off-chain" });
    const body = playerBody(await readJson(req), player);
    const r = await (url.pathname === "/rules/propose" ? chainRules.propose(body) : chainRules.repeal(body));
    send(res, r.status, r.body);
    return;
  }
  if (req.method === "GET" && url.pathname === "/player") {
    const player = auth.authenticate(sessionToken(req));
    if (url.searchParams.has("account") && url.searchParams.get("account") !== player.account) throw new AuthError(403, "You can only load your signed-in player.");
    const r = await playerView(chain, player.account);
    return send(res, r.status, r.body);
  }
  if (req.method === "GET" && url.pathname === "/rules") {
    const token = sessionToken(req);
    const account = token ? auth.authenticate(token).account : url.searchParams.get("account");
    return send(res, 200, await rulesOverview(world, chain, account, chainRules));
  }
  if (req.method === "GET" && url.pathname === "/chain") {
    const id = url.searchParams.get("battle");
    const ruleJob = url.searchParams.get("rule");
    const shopJob = url.searchParams.get("shop");
    const vowJob = url.searchParams.get("vow");
    if (!chain || !chainCfg) return send(res, 200, { enabled: false });
    if (vowJob) {
      const player = auth.authenticate(sessionToken(req));
      const receipt = chain.vowReceipt(vowJob);
      return receipt && receipt.account === player.account ? send(res, 200, receipt) : send(res, 404, { error: "no receipt for that vow" });
    }
    if (shopJob) {
      const receipt = chain.shopReceipt(shopJob);
      return receipt ? send(res, 200, receipt) : send(res, 404, { error: "no chain receipt for that trade" });
    }
    if (ruleJob) {
      const receipt = chain.ruleReceipt(ruleJob);
      return receipt ? send(res, 200, receipt) : send(res, 404, { error: "no chain receipt for that rule" });
    }
    if (!id) {
      const { network, gameAccount, itemsContract, riteContract, guestAccount } = chainCfg;
      return send(res, 200, { enabled: true, network, gameAccount, itemsContract, riteContract, guestAccount });
    }
    const receipt = chain.receipt(id);
    return receipt ? send(res, 200, receipt) : send(res, 404, { error: "no chain receipt for that battle" });
  }
  if (req.method === "GET" && url.pathname === "/health") return send(res, 200, { ok: true, tick: world.tick });
  if (req.method === "GET" && url.pathname === "/curses") return send(res, 200, curseCatalog());
  if (req.method === "GET" && url.pathname === "/state") return send(res, 200, summarize(world));
  if (req.method === "GET" && url.pathname === "/events") {
    const since = Number(url.searchParams.get("since") ?? -1);
    return send(res, 200, world.events.filter((e) => e.tick > since));
  }
  if (req.method === "POST" && url.pathname === "/tick" && DEV) {
    auth.authenticate(sessionToken(req));
    step(world, rng);
    return send(res, 200, summarize(world));
  }
  send(res, 404, { error: "not found" });
}
createServer((req, res) => {
  void handle(req, res).catch((e) => {
    if (res.headersSent || res.destroyed) return;
    const status = e instanceof AuthError ? e.status : e instanceof Error && ["body is not JSON", "body too large"].includes(e.message) ? 400 : 500;
    if (status === 500) console.warn("Request failed (details withheld from clients)");
    send(res, status, { error: status === 500 ? "The request could not be completed." : (e as Error).message });
  });
}).listen(PORT, HOST, () => {
  console.log(`Grudgebound server on http://localhost:${PORT}  (seed ${SEED}, 1 tick every ${TICK_MS} ms)`);
  console.log(chainCfg
    ? `Chain: ${chainCfg.network} via ${chainCfg.gameAccount} (items ${chainCfg.itemsContract}, rite ${chainCfg.riteContract}; key from ${chainCfg.keySource})`
    : "Chain: off (no NEAR_* accounts or key; battles stay off-chain)");
  chainRules?.syncFromChain().then(
    (n) => console.log(`Rules: ${n} in force on ${chainCfg?.riteContract} loaded into the world`),
    (e: Error) => console.warn(`Rules: could not read the rite contract (${e.message}); starting without chain rules`),
  );
  // Rules passed or repealed on the contract by anyone else (another server, the CLI) reach the world within a minute.
  if (chainRules) {
    setInterval(() => {
      chainRules.syncFromChain().catch((e: Error) => console.warn(`Rules: sync with the rite contract failed (${e.message})`));
    }, RULE_SYNC_MS).unref();
  }
  console.log(llm ? `Merchant tooltips: NEAR AI Cloud (${llm.model})` : "Merchant tooltips: formula reasons (no NEAR_AI_* settings in .env)");
  sweepVows();
  setInterval(sweepVows, Math.max(1000, Math.min(TICK_MS, 30_000))).unref();
});

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
 *   GET /chain?rule=ID   -> that proposal's on-chain receipt
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
 * Next (Phase 2-3): player sessions, relayer for meta transactions, reading active rules from the Rite contract.
 */
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";
import {
  createWorld, createRng, step, summarize, log,
  DEFAULT_REAL_MS_PER_TICK, DEMO_SEED,
} from "@grudgebound/shared";
import { llmConfigFromEnv, priceTooltip, type Tooltip } from "@grudgebound/agents";
import { BattleSessions, curseCatalog, type TurnAnswer } from "./battles";
import { ChainWriter, chainConfigFromEnv, custodialKey, type BattleRecord } from "./chain";
import { ChainRules, rulesOverview } from "./rules";
import { playerView } from "./player";
import { describeChainEvent } from "./chainEvents";
import { BattleArchive } from "./battleArchive";
import { Shop } from "./shop";

const PORT = Number(process.env.PORT ?? 8787);
const TICK_MS = Number(process.env.TICK_MS ?? DEFAULT_REAL_MS_PER_TICK);
const SEED = Number(process.env.WORLD_SEED ?? DEMO_SEED);
const DEV = process.env.NODE_ENV !== "production";

const world = createWorld(SEED);
const rng = createRng(SEED);

setInterval(() => step(world, rng), TICK_MS);

const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type",
};

function send(res: ServerResponse, status: number, body: unknown) {
  res.writeHead(status, { "Content-Type": "application/json", ...CORS });
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
const chain = chainCfg ? new ChainWriter(chainCfg) : null;
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
const canSignFor = (a: string) => !!chainCfg && custodialKey(chainCfg, a) !== undefined;
const chainRules = chain && chainCfg ? new ChainRules(world, chain, canSignFor) : null;
const playerShop = chain && chainCfg ? new Shop(world, chain, chainCfg.gameAccount, canSignFor) : null;

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
/** Tooltips per merchant, recomputed once per tick so the LLM is not called on every request. */
const shopCache = new Map<string, { tick: number; body: unknown }>();

async function shop(merchantId: string): Promise<{ status: number; body: unknown }> {
  const m = world.merchants.find((x) => x.id === merchantId);
  if (!m) return { status: 404, body: { error: `no merchant "${merchantId}"`, merchants: world.merchants.map((x) => x.id) } };
  const hit = shopCache.get(m.id);
  if (hit && hit.tick === world.tick) return { status: 200, body: hit.body };
  const items: (Tooltip & { id: string; name: string; stock: number })[] = [];
  for (const item of m.items) {
    const t = await priceTooltip(world, m, item.id, llm);
    items.push({ id: item.id, name: item.name, stock: item.stock, ...t });
  }
  const body = { merchant: { id: m.id, name: m.name, ward: m.ward, closed: m.closed }, tick: world.tick, items };
  shopCache.set(m.id, { tick: world.tick, body });
  return { status: 200, body };
}

createServer((req, res) => {
  const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);
  if (req.method === "OPTIONS") {
    res.writeHead(204, CORS);
    return res.end();
  }
  if (req.method === "POST" && (url.pathname === "/battle/start" || url.pathname === "/battle/turn")) {
    readJson(req).then(
      (body) => {
        if (url.pathname === "/battle/start") {
          const r = battles.start(body);
          return send(res, r.status, r.body);
        }
        const r = battles.turn(body);
        if (r.status !== 200) return send(res, r.status, r.body);
        const t = r.body as TurnAnswer;
        const record = recordFromTurn(t);
        if (record) {
          try { archive.add(record, t.ward ?? "lantern"); } catch (e) { console.warn(`battle archive: ${(e as Error).message}`); }
        }
        return withChain(t, record).then((b) => send(res, 200, b));
      },
      (e: Error) => send(res, 400, { error: e.message }),
    );
    return;
  }
  if (req.method === "GET" && url.pathname === "/shop") {
    const merchantId = url.searchParams.get("merchant") ?? "broker";
    shop(merchantId).then(
      async (r) => {
        if (r.status !== 200 || !playerShop) return send(res, r.status, r.body);
        const extras = await playerShop.extras(merchantId, url.searchParams.get("account"));
        send(res, 200, { ...(r.body as object), ...extras });
      },
      (e: Error) => send(res, 500, { error: e.message }),
    );
    return;
  }
  if (req.method === "POST" && (url.pathname === "/shop/buy" || url.pathname === "/shop/sell")) {
    if (!playerShop) return send(res, 503, { error: "trades are settled on chain, and this server runs off-chain" });
    readJson(req).then(
      (body) => (url.pathname === "/shop/buy" ? playerShop.buy(body) : playerShop.sell(body)).then((r) => {
        // Stock and demand just changed: the next GET /shop recomputes prices (and Kessa's tooltip lines).
        const m = (body as { merchant?: unknown } | null)?.merchant;
        if (r.status === 200 && typeof m === "string") shopCache.delete(m);
        send(res, r.status, r.body);
      }),
      (e: Error) => send(res, 400, { error: e.message }),
    ).catch((e: Error) => send(res, 500, { error: e.message }));
    return;
  }
  if (req.method === "POST" && url.pathname === "/rules/propose") {
    if (!chainRules) return send(res, 503, { error: "rules go through the rite contract, and this server runs off-chain" });
    readJson(req).then(
      (body) => chainRules.propose(body).then((r) => send(res, r.status, r.body)),
      (e: Error) => send(res, 400, { error: e.message }),
    ).catch((e: Error) => send(res, 500, { error: e.message }));
    return;
  }
  if (req.method === "GET" && url.pathname === "/player") {
    playerView(chain, url.searchParams.get("account")).then(
      (r) => send(res, r.status, r.body),
      (e: Error) => send(res, 500, { error: e.message }),
    );
    return;
  }
  if (req.method === "GET" && url.pathname === "/rules") {
    rulesOverview(world, chain, url.searchParams.get("account")).then(
      (body) => send(res, 200, body),
      (e: Error) => send(res, 500, { error: e.message }),
    );
    return;
  }
  if (req.method === "GET" && url.pathname === "/chain") {
    const id = url.searchParams.get("battle");
    const ruleJob = url.searchParams.get("rule");
    const shopJob = url.searchParams.get("shop");
    if (!chain || !chainCfg) return send(res, 200, { enabled: false });
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
    step(world, rng);
    return send(res, 200, summarize(world));
  }
  send(res, 404, { error: "not found" });
}).listen(PORT, () => {
  console.log(`Grudgebound server on http://localhost:${PORT}  (seed ${SEED}, 1 tick every ${TICK_MS} ms)`);
  console.log(chainCfg
    ? `Chain: ${chainCfg.network} via ${chainCfg.gameAccount} (items ${chainCfg.itemsContract}, rite ${chainCfg.riteContract}; key from ${chainCfg.keySource})`
    : "Chain: off (no NEAR_* accounts or key; battles stay off-chain)");
  chainRules?.syncFromChain().then(
    (n) => console.log(`Rules: ${n} in force on ${chainCfg?.riteContract} loaded into the world`),
    (e: Error) => console.warn(`Rules: could not read the rite contract (${e.message}); starting without chain rules`),
  );
  console.log(llm ? `Merchant tooltips: NEAR AI Cloud (${llm.model})` : "Merchant tooltips: formula reasons (no NEAR_AI_* settings in .env)");
});

/**
 * Game server (Day 1 skeleton).
 *
 * Runs the shared tick engine in real time and exposes the world over HTTP:
 *   GET /health          -> { ok: true }
 *   GET /state           -> world summary (Grudge, merchants, leaderboard, rules)
 *   GET /events?since=N  -> events after tick N
 *   POST /battle         -> a player's finished battle { wild, omen, playerCurse, seed, actions, outcome, ward, by }.
 *                           The server replays it from seed + actions; only if the replay ends the same way
 *                           does the capture count: Grudge drains, the town log records it, points come back.
 *   POST /battle/start   -> { ward, playerCurse?, by? }: the server picks the curse and the seed, and returns the battle view
 *   POST /battle/turn    -> { battleId, action }: the server resolves the turn (see battles.ts; used by the Unreal client)
 *   GET /curses          -> every curse's id, name, emotion, omen, home Ward and signature move (no stats)
 *   GET /shop?merchant=broker -> that merchant's prices, each with a one-line reason in the merchant's voice
 *                           (NEAR AI Cloud if .env has a key, else the plain formula reason; cached per tick)
 *   POST /tick           -> advance one tick now (dev only)
 *
 * Next (Phase 2-3): player sessions, binding -> mint on testnet, relayer for meta transactions,
 * reading active rules from the Rite contract.
 */
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";
import {
  createWorld, createRng, step, summarize, recordCapture, replayBattle, curseById,
  DEFAULT_REAL_MS_PER_TICK, OMENS, WARDS,
  type Omen, type PlayerAction, type WardId,
} from "@grudgebound/shared";
import { llmConfigFromEnv, priceTooltip, type Tooltip } from "@grudgebound/agents";
import { BattleSessions, MAX_TURNS, curseCatalog } from "./battles";

const PORT = Number(process.env.PORT ?? 8787);
const TICK_MS = Number(process.env.TICK_MS ?? DEFAULT_REAL_MS_PER_TICK);
const SEED = Number(process.env.WORLD_SEED ?? 42);
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
const seenBattles = new Set<string>();
const battles = new BattleSessions(world, { seen: seenBattles });

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

const ACTION_KINDS = new Set(["attack", "ward", "seal", "item", "flee"]);

/** Checks a reported battle by replaying it. Returns the points it earned, or an error message. */
function checkBattle(body: unknown): { points: number } | { error: string; status: number } {
  const b = body as Record<string, unknown>;
  if (!b || typeof b !== "object") return { error: "expected a JSON object", status: 400 };
  const { wild, omen, playerCurse, seed, actions, outcome, ward, by } = b;
  if (typeof wild !== "string" || typeof playerCurse !== "string") return { error: "wild and playerCurse must be curse ids", status: 400 };
  try { curseById(wild); curseById(playerCurse); } catch { return { error: "unknown curse", status: 400 }; }
  if (typeof omen !== "string" || !(OMENS as readonly string[]).includes(omen)) return { error: "unknown omen", status: 400 };
  if (curseById(wild).omen !== omen) return { error: "omen does not match that curse", status: 400 };
  if (typeof ward !== "string" || !(WARDS as readonly string[]).includes(ward)) return { error: "unknown ward", status: 400 };
  if (typeof seed !== "number" || !Number.isInteger(seed) || seed < 0 || seed > 0xffffffff) return { error: "seed must be a 32-bit integer", status: 400 };
  if (!Array.isArray(actions) || actions.length === 0 || actions.length > MAX_TURNS) return { error: `actions must hold 1-${MAX_TURNS} turns`, status: 400 };
  if (!(actions.every((a) => a && typeof a === "object" && ACTION_KINDS.has((a as { kind: string }).kind)))) return { error: "unknown action", status: 400 };
  if (outcome !== "bound" && outcome !== "won") return { error: "only bindings and exorcisms are reported", status: 400 };
  const name = typeof by === "string" && by.trim() ? by.trim().slice(0, 40) : "A newcomer";

  const key = `${seed}:${wild}:${playerCurse}`;
  if (seenBattles.has(key)) return { error: "this battle was already recorded", status: 409 };

  let turns;
  try {
    turns = replayBattle({ wild, omen: omen as Omen, playerCurse }, seed, actions as PlayerAction[]);
  } catch (e) {
    return { error: `replay failed: ${e instanceof Error ? e.message : "bad actions"}`, status: 422 };
  }
  const last = turns.at(-1)?.state;
  if (!last || last.outcome !== outcome || turns.length !== actions.length) {
    return { error: "the replay does not end the way the report says", status: 422 };
  }

  seenBattles.add(key);
  const points = recordCapture(world, {
    ward: ward as WardId, speciesId: wild, omen: omen as Omen, verb: outcome === "bound" ? "bind" : "exorcise", by: name,
  });
  return { points };
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
        const r = url.pathname === "/battle/start" ? battles.start(body) : battles.turn(body);
        send(res, r.status, r.body);
      },
      (e: Error) => send(res, 400, { error: e.message }),
    );
    return;
  }
  if (req.method === "POST" && url.pathname === "/battle") {
    readJson(req).then(
      (body) => {
        const r = checkBattle(body);
        if ("error" in r) send(res, r.status, { points: null, error: r.error });
        else send(res, 200, r);
      },
      (e: Error) => send(res, 400, { points: null, error: e.message }),
    );
    return;
  }
  if (req.method === "GET" && url.pathname === "/shop") {
    shop(url.searchParams.get("merchant") ?? "broker").then(
      (r) => send(res, r.status, r.body),
      (e: Error) => send(res, 500, { error: e.message }),
    );
    return;
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
  console.log(llm ? `Merchant tooltips: NEAR AI Cloud (${llm.model})` : "Merchant tooltips: formula reasons (no NEAR_AI_* settings in .env)");
});

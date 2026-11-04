import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { createWorld, enactRule, quoteSellPrice, totalGrudge } from "@grudgebound/shared";
import { ChainWriter, type ChainOps } from "./chain";
import { Shop, type TradeAnswer } from "./shop";

const CFG = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet" };
const GAME = "gb.testnet";
const AFREEN = "afreen.gb.testnet";

/** A fake items contract: Ember balances, talismans, every call recorded. */
function fakeItems(ember: Record<string, number> = { [AFREEN]: 60, [GAME]: 0 }) {
  const calls: string[] = [];
  const talismans: Record<string, { token_id: string; owner: string; meta: { species: string; emotion: string; omen: string } }> = {};
  let next = 1;
  let failOn: string | null = null;
  const transfer = (from: string, to: string, token: string, amount: number) => {
    if (token === "ember") {
      if ((ember[from] ?? 0) < amount) throw new Error("not enough Ember");
      ember[from] = (ember[from] ?? 0) - amount;
      ember[to] = (ember[to] ?? 0) + amount;
    } else {
      const t = talismans[token];
      if (!t || t.owner !== from) throw new Error("not the owner");
      t.owner = to;
    }
  };
  const ops: ChainOps = {
    async call(_c, method, args, deposit) {
      await new Promise((r) => setTimeout(r, 2));
      calls.push(`game ${method}${deposit ? " +1y" : ""}`);
      if (method === failOn) throw new Error(`${method} refused`);
      if (method === "mint_talisman") {
        const id = `tal-${next++}`;
        const meta = args.meta as { species: string; emotion: string; omen: string };
        talismans[id] = { token_id: id, owner: args.to as string, meta };
        return { hash: `h${calls.length}`, result: id };
      }
      if (method === "mint_ember") ember[args.to as string] = (ember[args.to as string] ?? 0) + Number(args.amount);
      if (method === "mt_transfer") transfer(GAME, args.receiver_id as string, args.token_id as string, Number(args.amount));
      return { hash: `h${calls.length}`, result: null };
    },
    async relay(player, _c, method, args, deposit) {
      await new Promise((r) => setTimeout(r, 2));
      calls.push(`${player} ${method}${deposit ? " +1y" : ""}`);
      if (method === failOn) throw new Error(`${method} refused`);
      if (method === "mt_transfer") transfer(player, args.receiver_id as string, args.token_id as string, Number(args.amount));
      return { hash: `h${calls.length}`, result: null };
    },
    async view(_c, method, args) {
      if (method === "mt_balance_of") return String(ember[args.account_id as string] ?? 0);
      if (method === "talisman") return talismans[args.token_id as string] ?? null;
      throw new Error(method);
    },
  };
  return { calls, ember, talismans, ops, failOn: (m: string | null) => { failOn = m; } };
}

function setup(ember?: Record<string, number>) {
  const world = createWorld(7);
  const items = fakeItems(ember);
  const chain = new ChainWriter(CFG, items.ops);
  const shop = new Shop(world, chain, GAME, (a) => a.endsWith(".gb.testnet"), (xs) => xs[0]!);
  const broker = world.merchants.find((m) => m.id === "broker")!;
  return { world, items, chain, shop, broker };
}

const settle = () => new Promise((r) => setTimeout(r, 40));

describe("player shop", () => {
  test("buying a talisman: Ember to the game master, a talisman minted, stock and price move", async () => {
    const { items, chain, shop, broker, world } = setup();
    const it = broker.items.find((i) => i.id === "talisman-sorrow")!;
    const ctx = { wardGrudge: totalGrudge(world.wards.lantern.grudge), buyerIsOathbreaker: false };
    const before = quoteSellPrice(it, ctx).price;
    const stock = it.stock;
    const r = await shop.buy({ account: AFREEN, merchant: "broker", itemId: "talisman-sorrow" });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    const a = r.body as TradeAnswer;
    assert.equal(a.price, before);
    await settle();
    assert.deepEqual(items.calls, [`${AFREEN} mt_transfer +1y`, "game mint_talisman"]);
    const receipt = chain.shopReceipt(a.jobId)!;
    assert.equal(receipt.status, "done");
    assert.equal(receipt.tokenId, "tal-1");
    assert.equal(items.talismans["tal-1"]!.owner, AFREEN);
    assert.equal(items.talismans["tal-1"]!.meta.emotion, "sorrow");
    assert.equal(items.ember[AFREEN], 60 - before);
    assert.equal(items.ember[GAME], before);
    const after = broker.items.find((i) => i.id === "talisman-sorrow")!;
    assert.equal(after.stock, stock - 1);
    assert.ok(quoteSellPrice(after, ctx).price >= before, "demand pushes the price up (or holds it)");
    assert.match(world.events.at(-1)!.text, /Afreen bought a .* talisman from the Talisman Broker/);
  });

  test("selling a talisman: handed to the game master, paid the buy-back less the trade tax", async () => {
    const { items, chain, shop, broker, world } = setup({ [AFREEN]: 0, [GAME]: 0 });
    items.talismans["tal-9"] = { token_id: "tal-9", owner: AFREEN, meta: { species: "weepwick", emotion: "sorrow", omen: "whisper" } };
    enactRule(world, { author: "x", authorName: "X", templateId: "R7", params: { percent: 10, payee: "burn" }, cost: 50 });
    const purse = broker.ember;
    const r = await shop.sell({ account: AFREEN, merchant: "broker", tokenId: "tal-9" });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    const a = r.body as TradeAnswer;
    await settle();
    assert.equal(chain.shopReceipt(a.jobId)!.status, "done");
    assert.equal(items.talismans["tal-9"]!.owner, GAME);
    assert.equal(items.ember[AFREEN], a.price);
    assert.deepEqual(items.calls, [`${AFREEN} mt_transfer +1y`, "game mint_ember", "game mt_transfer +1y"], "treasury topped up, then paid");
    assert.ok(broker.ember < purse);
    assert.match(world.events.at(-1)!.text, /sold a Weepwick talisman \(tal-9\)/);
  });

  test("refusals before anything is sent", async () => {
    const { items, shop } = setup({ [AFREEN]: 5 });
    items.talismans["tal-9"] = { token_id: "tal-9", owner: "someone.testnet", meta: { species: "weepwick", emotion: "sorrow", omen: "whisper" } };
    const poor = await shop.buy({ account: AFREEN, merchant: "broker", itemId: "talisman-rage" });
    assert.equal(poor.status, 402);
    assert.match((poor.body as { error: string }).error, /Rage talismans cost \d+ Ember; Afreen has 5/);
    assert.equal((await shop.buy({ account: AFREEN, merchant: "broker", itemId: "binding-seal" })).status, 400);
    assert.equal((await shop.buy({ account: AFREEN, merchant: "nobody", itemId: "talisman-rage" })).status, 404);
    assert.equal((await shop.buy({ account: "bob.testnet", merchant: "broker", itemId: "talisman-rage" })).status, 403);
    assert.equal((await shop.sell({ account: AFREEN, merchant: "broker", tokenId: "tal-9" })).status, 403);
    assert.equal((await shop.sell({ account: AFREEN, merchant: "broker", tokenId: "tal-404" })).status, 404);
    assert.deepEqual(items.calls, []);
  });

  test("a chain failure puts the merchant's stock and purse back", async () => {
    const { items, shop, broker } = setup();
    items.failOn("mint_talisman");
    const it = () => broker.items.find((i) => i.id === "talisman-dread")!;
    const stock = it().stock;
    const purse = broker.ember;
    const r = await shop.buy({ account: AFREEN, merchant: "broker", itemId: "talisman-dread" });
    assert.equal(r.status, 200);
    await settle();
    assert.equal(it().stock, stock);
    assert.equal(broker.ember, purse);
  });

  test("the shop screen's extras: chain Ember and buy-back prices", async () => {
    const { shop } = setup();
    const x = await shop.extras("broker", AFREEN);
    assert.equal(x.ember, 60);
    assert.deepEqual(Object.keys(x.buyBack).sort(), ["talisman-dread", "talisman-rage", "talisman-sorrow"]);
    assert.ok(Object.values(x.buyBack).every((p) => p > 0));
  });
});

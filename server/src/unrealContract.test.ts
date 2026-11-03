/**
 * The fields the Unreal client reads (unreal/Plugins/GrudgeboundClient/.../GbJson.cpp).
 * If a server answer is renamed or reshaped, this fails here instead of the Unreal game silently showing zeros.
 * Change GbJson.cpp in the same pull request as any change to these lists.
 */
import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { createWorld, summarize } from "@grudgebound/shared";
import { BattleSessions, curseCatalog, type StartAnswer, type TurnAnswer } from "./battles";
import { ChainWriter, type BattleRecord } from "./chain";
import { rulesOverview } from "./rules";
import { Shop } from "./shop";
import { playerView } from "./player";

const VIEW = ["turn", "outcome", "player", "wild", "items", "chances", "canUse"];
const SIDE = ["speciesId", "name", "emotion", "omen", "hp", "maxHp"];
const ITEMS = ["salve", "gilded"];
const CHANCES = ["seal", "gildedSeal", "flee"];
const CAN_USE = ["attack", "ward", "seal", "gildedSeal", "salve", "flee"];

function hasKeys(obj: unknown, keys: string[], where: string) {
  assert.ok(obj && typeof obj === "object", `${where} is not an object`);
  for (const k of keys) assert.ok(k in (obj as object), `${where}.${k} is missing (read by GbJson.cpp)`);
}

function checkView(v: unknown, where: string) {
  hasKeys(v, VIEW, where);
  const view = v as Record<string, unknown>;
  hasKeys(view.player, SIDE, `${where}.player`);
  hasKeys(view.wild, SIDE, `${where}.wild`);
  hasKeys(view.items, ITEMS, `${where}.items`);
  hasKeys(view.chances, CHANCES, `${where}.chances`);
  hasKeys(view.canUse, CAN_USE, `${where}.canUse`);
}

describe("Unreal client contract", () => {
  test("GET /state has what UGbApiSubsystem parses", () => {
    const s = summarize(createWorld(7)) as unknown as Record<string, unknown>;
    hasKeys(s, ["tick", "day", "hour", "night", "wards", "merchants", "leaderboard", "rules", "burnedEmber"], "state");
    const ward = (s.wards as unknown[])[0];
    hasKeys(ward, ["id", "name", "grudge", "total", "wild"], "state.wards[0]");
    hasKeys((ward as Record<string, unknown>).grudge, ["rage", "sorrow", "dread"], "state.wards[0].grudge");
    hasKeys((s.merchants as unknown[])[0], ["id", "name", "ward", "ember", "closed", "prices"], "state.merchants[0]");
    hasKeys((s.leaderboard as unknown[])[0], ["id", "name", "kind", "points", "ember", "talismans"], "state.leaderboard[0]");
    assert.ok((s.wards as { id: string }[]).some((w) => w.id === "lantern"), "Unreal's default ward is lantern");
  });

  test("GET /curses has what the Talisman Case shows", () => {
    for (const c of curseCatalog()) {
      hasKeys(c, ["id", "name", "emotion", "omen", "home", "bornFrom", "signatureMove"], `curse ${c.id}`);
      hasKeys(c.signatureMove, ["name", "effect"], `curse ${c.id}.signatureMove`);
    }
  });

  test("battle start and turns have what the battle screen draws, with the body Unreal sends", () => {
    let seed = 99;
    const sessions = new BattleSessions(createWorld(7), { newSeed: () => seed++ });
    // Exactly the body GbJson::StartBody writes.
    const r = sessions.start({ ward: "lantern", playerCurse: "cinder-grin", by: "A newcomer" });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    const start = r.body as StartAnswer;
    hasKeys(start, ["battleId", "ward", "wild", "matchup", "view", "lines"], "start");
    hasKeys(start.wild, ["id", "name", "emotion", "omen"], "start.wild");
    checkView(start.view, "start.view");
    assert.equal(start.view.outcome, null, "outcome is null while the battle runs (GbJson treats null as None)");

    // Every action body GbJson::TurnBody can write is accepted.
    const bodies = [
      { kind: "ward" }, { kind: "item", item: "salve" }, { kind: "seal", gilded: true }, { kind: "seal" }, { kind: "flee" },
    ];
    for (const action of bodies) {
      const t = sessions.turn({ battleId: start.battleId, action });
      assert.notEqual(t.status, 400, `action ${JSON.stringify(action)} was refused as malformed`);
      if (t.status !== 200) continue;
      const turn = t.body as TurnAnswer;
      hasKeys(turn, ["battleId", "action", "lines", "view", "outcome"], "turn");
      checkView(turn.view, "turn.view");
      if (turn.outcome) {
        hasKeys(turn, ["points", "seed"], "last turn");
        assert.equal(typeof turn.seed, "number");
        return;
      }
    }
    // Still going: attack until it ends and check the final answer.
    for (let i = 0; i < 200; i++) {
      const t = sessions.turn({ battleId: start.battleId, action: { kind: "attack" } });
      assert.equal(t.status, 200, JSON.stringify(t.body));
      const turn = t.body as TurnAnswer;
      if (turn.outcome) {
        hasKeys(turn, ["points", "seed"], "last turn");
        return;
      }
    }
    assert.fail("battle never ended");
  });

  test("the chain receipt (turn.chain and GET /chain?battle=) has what GbJson::ChainReceipt reads", async () => {
    const w = new ChainWriter(
      { network: "testnet", itemsContract: "items.t", riteContract: "rite.t", guestAccount: "guest.t" },
      async (_c, method) => {
        await new Promise((res) => setTimeout(res, 5));
        return { hash: "h", result: method === "mint_talisman" ? "tal-1" : null };
      },
    );
    const r: BattleRecord = {
      battleId: "b", account: "guest.t", outcome: "bound", points: 5, seed: 1, actions: [{ kind: "seal" }],
      wild: { speciesId: "hushmoth", emotion: "dread", omen: "whisper" }, playerCurse: "cinder-grin",
    };
    const pending = await w.record(r);
    hasKeys(pending, ["status", "network", "account", "txs"], "chain (pending)");
    assert.equal(pending.status, "pending", "the last turn answers before the chain does; Unreal polls on \"pending\"");
    await new Promise((res) => setTimeout(res, 50));
    const done = w.receipt("b")!;
    assert.equal(done.status, "done");
    hasKeys(done, ["tokenId"], "chain (done)");
    for (const tx of done.txs) hasKeys(tx, ["kind", "url"], "chain.txs[]");
    assert.ok(done.txs.some((t) => t.kind === "mint"), "Unreal shows the mint link");
  });

  test("GET /rules has what GbJson::RulesOverview reads", async () => {
    const o = await rulesOverview(createWorld(7), null, null);
    hasKeys(o, ["chain", "cost", "points", "templates", "active"], "rules");
    for (const t of o.templates) {
      hasKeys(t, ["id", "text", "cruel", "params"], `rules.templates ${t.id}`);
      assert.ok(t.params.length <= 3, `${t.id}: the Unreal panel has 3 parameter rows`);
      for (const p of t.params) {
        hasKeys(p, ["key", "options"], `${t.id}.params`);
        for (const opt of p.options) hasKeys(opt, ["value", "label"], `${t.id}.${p.key}.options[]`);
        assert.ok(t.text.includes(`{${p.key}}`), `${t.id}: Unreal fills {${p.key}} in the text`);
      }
    }
  });

  test("the shop extras and a trade answer have what GbJson::Shop / GbJson::Trade read", async () => {
    const ember: Record<string, number> = { "afreen.gb.t": 60 };
    const chain = new ChainWriter(
      { network: "testnet", itemsContract: "items.gb.t", riteContract: "rite.gb.t", guestAccount: "guest.gb.t" },
      {
        call: async () => ({ hash: "h", result: "tal-1" }),
        relay: async () => ({ hash: "h", result: null }),
        view: async (_c, method, args) => (method === "mt_balance_of" ? String(ember[args.account_id as string] ?? 0) : null),
      },
    );
    const shop = new Shop(createWorld(7), chain, "gb.t", () => true);
    const x = await shop.extras("broker", "afreen.gb.t");
    hasKeys(x, ["ember", "buyBack"], "shop extras");
    assert.equal(typeof x.ember, "number", "Unreal reads ember as a number (-1 when null)");
    assert.ok(Object.keys(x.buyBack).every((k) => k.startsWith("talisman-")), "buyBack is keyed by item id");
    const r = await shop.buy({ account: "afreen.gb.t", merchant: "broker", itemId: "talisman-dread" });
    assert.equal(r.status, 200, JSON.stringify(r.body));
    hasKeys(r.body, ["jobId", "price", "text", "speciesId", "omen", "chain"], "trade");
    hasKeys((r.body as { chain: object }).chain, ["status", "network", "account", "txs"], "trade.chain");
  });

  test("GET /player has what GbJson::Player reads", async () => {
    const chain = new ChainWriter(
      { network: "testnet", itemsContract: "items.gb.t", riteContract: "rite.gb.t", guestAccount: "guest.gb.t" },
      {
        view: async (_c, method) =>
          method === "get_points" ? 5 : method === "mt_balance_of" ? "60"
            : { talismans: [{ token_id: "tal-1", owner: "a.gb.t", meta: { species: "hushmoth", emotion: "dread", omen: "whisper", level: 1, wins: 0 }, purse: "0" }], next_index: null },
      },
    );
    const r = await playerView(chain, "a.gb.t");
    hasKeys(r.body, ["account", "name", "chain", "points", "ember", "talismans", "explorerUrl"], "player");
    for (const t of (r.body as { talismans: object[] }).talismans) hasKeys(t, ["tokenId", "speciesId", "emotion", "omen", "level", "wins", "purse"], "player.talismans[]");
  });
});

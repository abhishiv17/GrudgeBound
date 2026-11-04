import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { ChainWriter, type ChainTalisman } from "./chain";
import { playerView, type PlayerView } from "./player";

const CFG = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet" };
const AFREEN = "afreen.gb.testnet";

function tal(n: number, owner: string): ChainTalisman {
  return { token_id: `tal-${n}`, owner, meta: { species: "weepwick", emotion: "sorrow", omen: "whisper", level: 1, wins: n }, purse: String(n * 10) };
}

describe("GET /player", () => {
  test("reads points, Ember and talismans from chain, paging talismans_of", async () => {
    const pages: Record<number, { talismans: ChainTalisman[]; next_index: number | null }> = {
      1: { talismans: [tal(1, AFREEN)], next_index: 201 },
      201: { talismans: [tal(250, AFREEN)], next_index: null },
    };
    const chain = new ChainWriter(CFG, {
      view: async (_c, method, args) => {
        if (method === "get_points") return 55;
        if (method === "mt_balance_of") return "51";
        if (method === "talismans_of") return pages[args.from_index as number] ?? null;
        throw new Error(method);
      },
    });
    const r = await playerView(chain, AFREEN);
    assert.equal(r.status, 200);
    const p = r.body as PlayerView;
    assert.equal(p.name, "Afreen");
    assert.equal(p.chain, true);
    assert.equal(p.points, 55);
    assert.equal(p.ember, 51);
    assert.deepEqual(p.talismans.map((t) => t.tokenId), ["tal-1", "tal-250"]);
    assert.equal(p.talismans[1]!.purse, 2500);
    assert.equal(p.explorerUrl, `https://testnet.nearblocks.io/address/${AFREEN}`);
  });

  test("off-chain servers answer with chain:false; bad ids are refused; chain errors are 502", async () => {
    const off = await playerView(null, AFREEN);
    assert.equal((off.body as PlayerView).chain, false);
    assert.equal((await playerView(null, "Not An Id!")).status, 400);
    const broken = new ChainWriter(CFG, { view: async () => { throw new Error("rpc down"); } });
    const r = await playerView(broken, AFREEN);
    assert.equal(r.status, 502);
  });
});

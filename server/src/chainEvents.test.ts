import { test, describe } from "node:test";
import assert from "node:assert/strict";
import { ChainWriter } from "./chain";
import { describeChainEvent, eventsIn, type ChainEvent, type RawChainEvent } from "./chainEvents";

const GAME = "gb.testnet";
const log = (standard: string, event: string, data: object) => `EVENT_JSON:${JSON.stringify({ standard, version: "1.0.0", event, data: [data] })}`;

describe("contract events (P2-C6)", () => {
  test("eventsIn reads EVENT_JSON logs from every receipt, skipping other logs and junk", () => {
    const res = {
      receipts_outcome: [
        { outcome: { executor_id: "afreen.gb.testnet", logs: [] } },
        { outcome: { executor_id: "items.gb.testnet", logs: ["hello", log("nep245", "mt_transfer", { old_owner_id: "afreen.gb.testnet", new_owner_id: GAME, token_ids: ["ember"], amounts: ["30"] }), "EVENT_JSON:{oops"] } },
        { outcome: { executor_id: "items.gb.testnet", logs: [log("nep245", "mt_mint", { owner_id: "afreen.gb.testnet", token_ids: ["tal-6"], amounts: ["1"] })] } },
      ],
    };
    const ev = eventsIn(res);
    assert.deepEqual(ev.map((e) => `${e.contract} ${e.event}`), ["items.gb.testnet mt_transfer", "items.gb.testnet mt_mint"]);
  });

  test("Kessa's words for each event", () => {
    const say = (event: string, data: object) => describeChainEvent({ contract: "c", standard: "s", event, data: [data as Record<string, unknown>] }, GAME)?.text;
    assert.equal(say("mt_mint", { owner_id: "afreen.gb.testnet", token_ids: ["tal-6"], amounts: ["1"] }), "NEAR: talisman tal-6 sealed for Afreen.");
    assert.equal(say("mt_mint", { owner_id: GAME, token_ids: ["ember"], amounts: ["1000"] }), "NEAR: 1000 Ember minted to the game master.");
    assert.equal(say("mt_transfer", { old_owner_id: "afreen.gb.testnet", new_owner_id: GAME, token_ids: ["ember"], amounts: ["30"] }), "NEAR: Afreen sent 30 Ember to the game master.");
    assert.equal(say("rule_proposed", { id: 2, template_id: "R8", author: "akshay.gb.testnet", cost: 50 }), "NEAR: Akshay paid 50 points to propose rule 2 (R8).");
    assert.equal(say("rule_resolved", { id: 2, accepted: false, reason: "Contradicts it" }), "NEAR: rule 2 rejected: Contradicts it (points refunded).");
    assert.equal(say("rule_resolved", { id: 3, accepted: true }), "NEAR: rule 3 is law.");
    assert.match(say("battle", { battle_id: "0123456789", seed: 42 })!, /recorded battle 01234567 \(seed 42\)/);
    assert.equal(describeChainEvent({ contract: "c", standard: "s", event: "unknown", data: [] }, GAME), null);
  });

  test("the writer hands every write's events, with the tx link, to its listeners", async () => {
    const raw: RawChainEvent = { contract: "items.gb.testnet", standard: "nep245", event: "mt_mint", data: [{ owner_id: "guest.gb.testnet", token_ids: ["tal-1"], amounts: ["1"] }] };
    const chain = new ChainWriter(
      { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: "guest.gb.testnet" },
      { call: async (_c: string, method: string) => (method === "mint_talisman" ? { hash: "H1", result: "tal-1", events: [raw] } : { hash: "H0", result: null }), relay: async () => ({ hash: "H2", result: null }) },
    );
    const seen: ChainEvent[] = [];
    chain.onEvents((e) => seen.push(...e));
    await chain.record({
      battleId: "b", account: "guest.gb.testnet", outcome: "bound", points: 5, seed: 1, actions: [{ kind: "seal" }],
      wild: { speciesId: "hushmoth", emotion: "dread", omen: "whisper" }, playerCurse: "cinder-grin",
    }, 1000);
    assert.equal(seen.length, 1, "the mint's event (submit_battle logged none here)");
    assert.equal(seen[0]!.txHash, "H1");
    assert.equal(seen[0]!.url, "https://testnet.nearblocks.io/txns/H1");
  });
});

import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, sep } from "node:path";
import { spawn } from "node:child_process";
import { createServer } from "node:net";
import { randomBytes } from "node:crypto";
import { setTimeout as delay } from "node:timers/promises";
import {
  createBattle, createRng, createWorld, DEFAULT_PLAYER_CURSE, quoteSellPrice, step, totalGrudge, resolveTurn,
} from "@grudgebound/shared";
import { AuthError } from "./auth";
import { ChainWriter, battleLogHash, type BattleRecord, type ChainTalisman } from "./chain";
import { BattleSessions, type StartAnswer, type TurnAnswer } from "./battles";
import { Satchels, STARTER_KIT, STARTER_TOKEN_ID, menderQuote, type Supply } from "./satchel";
import { Shop, type TradeAnswer } from "./shop";
import { checkBattle } from "./replay";

const ACCOUNT = "alice.gb.testnet", OTHER = "bob.gb.testnet", GAME = "gb.testnet";
const cfg = { network: "testnet", itemsContract: "items.gb.testnet", riteContract: "rite.gb.testnet", guestAccount: ACCOUNT };

test("fifty rests leave the shared event feed unchanged", async () => {
  const s = setup(); const before = structuredClone(s.world.events);
  for (let i = 0; i < 50; i++) await s.satchels.rest(ACCOUNT);
  assert.deepEqual(s.world.events, before);
});

test("arrival choices persist, validate names, isolate accounts and cannot reroll", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-arrival-")); t.after(() => rmSync(dir, { recursive: true, force: true }));
  const file = join(dir, "satchels.json"), world = createWorld(2);
  const s = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  const choice = { name: "  Mira  Vale ", look: "feminine", pronouns: "they", starter: "weepwick", attack: 999, level: 99 };
  await s.view(ACCOUNT);
  assert.equal(s.profile(ACCOUNT).legacyStarter, false, "viewing pockets does not lock the new choice");
  for (const name of ["x", "x".repeat(25), "<script>", "Mira\nVale", "\u200bMira", "123"])
    await assert.rejects(s.completeProfile(ACCOUNT, { ...choice, name }), /name|letters/);
  await assert.rejects(s.completeProfile(ACCOUNT, { ...choice, starter: "riot-bell" }), /three partners/);
  const answers = await Promise.all(Array.from({ length: 8 }, () => s.completeProfile(ACCOUNT, choice)));
  assert.ok(answers.every(a => a.profile.starter === "weepwick"));
  assert.equal(answers[0]!.profile.name, "Mira Vale");
  assert.equal(answers[0]!.supplies.blank, 3);
  assert.equal((await s.partner(ACCOUNT, "starter")).maxHp, 44);
  assert.equal(s.profile(OTHER).complete, false);
  await assert.rejects(s.completeProfile(ACCOUNT, { ...choice, starter: "hushmoth" }), /cannot be chosen again/);
  const restored = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  assert.equal(restored.profile(ACCOUNT).name, "Mira Vale");
  assert.equal(restored.profile(ACCOUNT).look, "feminine");
  assert.equal((await restored.partner(ACCOUNT, "starter")).speciesId, "weepwick");
  await restored.partner(OTHER, "starter");
  await assert.rejects(restored.completeProfile(OTHER, choice), /existing partner/);
  assert.equal((await restored.completeProfile(OTHER, { ...choice, starter: DEFAULT_PLAYER_CURSE })).profile.legacyStarter, true);
});

test("starter mint lost response resumes the same choice after restart without another grant", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-starter-")); t.after(() => rmSync(dir, { recursive: true, force: true }));
  const file = join(dir, "satchels.json"), world = createWorld(2);
  const tokens: ChainTalisman[] = []; let loseReply = true, calls = 0;
  const chain = new ChainWriter(cfg, {
    view: async (_c, method) => method === "talismans_of" ? { talismans: tokens, next_index: null } : 0,
    call: async (_c: string, method: string, args: Record<string, unknown>) => {
      assert.equal(method, "mint_starter"); calls++;
      if (!tokens.length) tokens.push({ token_id: "tal-1", owner: ACCOUNT, meta: { species: args.species as string, emotion: "dread", omen: "whisper", level: 1, wins: 0 }, purse: "0" });
      if (loseReply) { loseReply = false; throw new Error("response lost after execution"); }
      return { hash: "starter-proof", result: "tal-1" };
    },
  });
  const choice = { name: "Ari", look: "masculine", pronouns: "he", starter: "hushmoth" };
  const s = new Satchels(world, chain, { file, mintStarters: true }); await s.view(ACCOUNT);
  await assert.rejects(s.completeProfile(ACCOUNT, choice), /response lost/);
  await assert.rejects(s.partner(ACCOUNT, "starter"), /still being recorded/);
  const resumed = new Satchels(world, chain, { file, mintStarters: true });
  await assert.rejects(resumed.completeProfile(ACCOUNT, { ...choice, starter: "weepwick" }), /cannot be chosen again/);
  const done = await resumed.completeProfile(ACCOUNT, choice);
  assert.equal(done.profile.mint, "done"); assert.equal(done.profile.tokenId, "tal-1");
  assert.deepEqual(done.carried, ["tal-1"]); assert.equal(tokens.length, 1);
  await resumed.completeProfile(ACCOUNT, choice); assert.equal(calls, 2, "completed profile never asks to mint again");
  assert.equal((await resumed.partner(ACCOUNT, "tal-1")).speciesId, "hushmoth");
});

test("exploration rewards are once per account, ordered, and survive restart", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-discovery-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const file = join(dir, "satchels.json"), world = createWorld(2);
  const s = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  await assert.rejects(s.explore(ACCOUNT, "storehouse-chest"), isStatus(403));
  await assert.rejects(s.equip(ACCOUNT, "etched-focus"), isStatus(403));
  await assert.rejects(s.explore(ACCOUNT, "fake-discovery"), isStatus(400));
  await Promise.all(Array.from({ length: 12 }, () => s.explore(ACCOUNT, "alley-parcel")));
  assert.deepEqual(s.supplies(ACCOUNT), { blank: 5, salve: 2, gilded: 0 });
  await s.explore(ACCOUNT, "lamp-kit"); // find before accepting
  await s.explore(ACCOUNT, "lamplighter");
  await s.equip(ACCOUNT, "worn-focus");
  assert.equal(s.equipment(ACCOUNT)?.attack, 1);
  await s.explore(ACCOUNT, "reed-bundle");
  const won = createBattle({ wild: "cinder-grin" }); won.outcome = "won";
  s.applyTurn(ACCOUNT, "starter", "before", { kind: "ward" }, won, "tide");
  assert.ok(!(await s.explore(ACCOUNT, "net-mender")).exploration.flags.includes("net-done"));
  const fled = { ...won, outcome: "fled" as const };
  s.applyTurn(ACCOUNT, "starter", "flee", { kind: "flee" }, fled, "tide");
  s.applyTurn(ACCOUNT, "starter", "elsewhere", { kind: "ward" }, won, "lantern");
  assert.ok(!(await s.view(ACCOUNT)).exploration.flags.includes("tide-cleared"));
  s.applyTurn(ACCOUNT, "starter", "victory", { kind: "attack" }, won, "tide");
  await s.explore(ACCOUNT, "net-mender");
  await s.explore(ACCOUNT, "receipt-clerk");
  await s.explore(ACCOUNT, "storehouse-door");
  await s.explore(ACCOUNT, "storehouse-chest");
  const reloaded = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  const complete = await reloaded.explore(ACCOUNT, "storehouse-chest");
  assert.equal(complete.supplies.gilded, 1);
  assert.equal(complete.exploration.gear.length, 3);
  assert.equal(complete.exploration.equipped, "worn-focus");
  assert.ok(complete.exploration.journal.every(j => j.status === "complete"));
  assert.ok(complete.exploration.notes.some(n => n.id === "storehouse-record"));
  assert.deepEqual((await reloaded.view(OTHER)).exploration.flags, []);
  assert.deepEqual(reloaded.supplies(OTHER), STARTER_KIT);
});

test("enabling starter settlement preserves the virtual partner's ink and welcome supplies", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-starter-upgrade-")); t.after(() => rmSync(dir, { recursive: true, force: true }));
  const file = join(dir, "satchels.json"), world = createWorld(2);
  const choice = { name: "Ari", look: "feminine", pronouns: "they", starter: "hushmoth" };
  const before = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  await before.completeProfile(ACCOUNT, choice);
  const hurt = createBattle({ wild: "cinder-grin", playerCurse: "hushmoth" }); hurt.player.hp = 9;
  before.applyTurn(ACCOUNT, "starter", "injury", { kind: "ward" }, hurt);
  let calls = 0;
  const chain = new ChainWriter(cfg, {
    view: async (_c, method) => method === "talismans_of" ? { talismans: calls ? [{ token_id: "tal-1", owner: ACCOUNT, meta: { species: "hushmoth", emotion: "dread", omen: "whisper", level: 1, wins: 0 }, purse: "0" }] : [], next_index: null } : 0,
    call: async () => { calls++; return { hash: "starter-proof", result: "tal-1" }; },
  });
  const after = new Satchels(world, chain, { file, mintStarters: true, starterKit: STARTER_KIT });
  const settled = await after.completeProfile(ACCOUNT, choice);
  assert.equal(settled.ink["tal-1"]!.hp, 9);
  assert.deepEqual(settled.supplies, STARTER_KIT);
  assert.deepEqual(settled.carried, ["tal-1"]);
  assert.equal(settled.profile.mint, "done");
  await after.completeProfile(ACCOUNT, choice); assert.equal(calls, 1);
});

test("legacy Satchel migration preserves supplies and starter-kit history", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-discovery-migration-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const file = join(dir, "satchels.json"), world = createWorld(2);
  const s = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  await s.view(ACCOUNT);
  const saved = JSON.parse(readFileSync(file, "utf8")); saved.version = 1;
  for (const [, p] of saved.players) { delete p.exploration; delete p.pendingTideBindings; }
  writeFileSync(file, JSON.stringify(saved));
  const old = new Satchels(world, null, { file, starterKit: STARTER_KIT });
  assert.deepEqual((await old.view(ACCOUNT)).supplies, STARTER_KIT);
  assert.equal(old.profile(ACCOUNT).legacyStarter, true);
  assert.equal(old.profile(ACCOUNT).complete, false, "legacy users still choose name, look and pronouns");
  await old.explore(ACCOUNT, "dock-tin");
  assert.equal(JSON.parse(readFileSync(file, "utf8")).version, 3);
  assert.equal(old.supplies(ACCOUNT).salve, 2);
});

test("Tide binding objective waits for mint settlement and failed mint never completes it", async () => {
  const s = new Satchels(createWorld(2), null, { starterKit: STARTER_KIT });
  await s.explore(ACCOUNT, "net-mender");
  const bound = createBattle({ wild: "cinder-grin" }); bound.outcome = "bound";
  s.applyTurn(ACCOUNT, "starter", "failed", { kind: "seal" }, bound, "tide");
  assert.ok(!(await s.view(ACCOUNT)).exploration.flags.includes("tide-cleared"));
  s.settleBinding(ACCOUNT, "failed", { status: "failed", error: "mint rejected" } as any);
  assert.ok(!(await s.view(ACCOUNT)).exploration.flags.includes("tide-cleared"));
  s.applyTurn(ACCOUNT, "starter", "minted", { kind: "seal" }, bound, "tide");
  s.settleBinding(ACCOUNT, "minted", { status: "failed", tokenId: "minted-token", error: "points failed" } as any);
  assert.ok((await s.view(ACCOUNT)).exploration.flags.includes("tide-cleared"));
});

test("equipment changes real battle damage, is frozen, and is included in replay proof", async () => {
  const s = setup();
  s.world.wards.lantern!.grudge = { rage: 0, sorrow: 1, dread: 0 };
  await s.satchels.explore(ACCOUNT, "lamp-kit"); await s.satchels.explore(ACCOUNT, "lamplighter");
  await s.satchels.equip(ACCOUNT, "worn-focus");
  const battles = new BattleSessions(s.world, { satchels: s.satchels, newSeed: () => 99 });
  const start = battles.start({ ward: "lantern", account: ACCOUNT, equipment: { attack: 999 } }, undefined, await s.satchels.partner(ACCOUNT)).body as StartAnswer;
  // Internal mutation simulates a changed catalog/loadout; the already-created fight keeps its snapshot.
  await s.satchels.equip(ACCOUNT, "");
  let end: TurnAnswer;
  do { end = battles.turn({ battleId: start.battleId, action: { kind: "attack" } }, ACCOUNT).body as TurnAnswer; } while (!end.outcome);
  assert.equal(end.initial?.equipment?.attack, 1);
  const r = record(end), logHash = battleLogHash(r);
  const check = checkBattle({ ...r, logHash, ward: "lantern", at: 0 });
  assert.ok(check.hashMatches && check.outcomeMatches);
  assert.notEqual(battleLogHash({ ...r, initial: { ...r.initial!, equipment: undefined } }), logHash);
  const plain = createBattle({ wild: start.wild.id, playerCurse: end.view.player.speciesId });
  const geared = createBattle({ wild: start.wild.id, playerCurse: end.view.player.speciesId, equipment: end.initial!.equipment });
  assert.equal(geared.player.attack, plain.player.attack + 1);
  assert.ok(resolveTurn(geared, { kind: "attack" }, createRng(4)).state.wild.hp
    < resolveTurn(plain, { kind: "attack" }, createRng(4)).state.wild.hp, "equipment changes damage with identical rolls");
});

function setup(file?: string) {
  const world = createWorld(7);
  const tokens: ChainTalisman[] = Array.from({ length: 7 }, (_, i) => ({ token_id: `tal-${i + 1}`, owner: ACCOUNT,
    meta: { species: DEFAULT_PLAYER_CURSE, emotion: "rage", omen: "whisper", level: 1, wins: 0 }, purse: "0" }));
  const balances: Record<string, number> = { [ACCOUNT]: 10_000, [OTHER]: 10_000 };
  let fail = "";
  const calls: string[] = [];
  const chain = new ChainWriter(cfg, {
    view: async (_c, method, args) => {
      if (method === "talismans_of") return { talismans: tokens.filter((t) => t.owner === args.account_id), next_index: null };
      if (method === "mt_balance_of") return String(balances[args.account_id as string] ?? 0);
      if (method === "get_points") return 17;
      if (method === "talisman") return tokens.find((t) => t.token_id === args.token_id) ?? null;
      throw new Error(method);
    },
    call: async (_c: string, method: string) => {
      calls.push(method);
      if (method === fail) throw new Error("Test contract refused");
      return { hash: "tx", result: method === "mint_talisman" ? "minted-token" : null };
    },
    relay: async (account, _c, method, args) => {
      await delay(2); calls.push(method);
      if (method === fail) throw new Error("Test payment refused");
      if (args.token_id === "ember") {
        const price = Number(args.amount);
        assert.ok(balances[account]! >= price);
        balances[account]! -= price;
      }
      return { hash: "pay", result: null };
    },
  });
  const options = { file, gameAccount: GAME, canSignFor: () => true };
  const satchels = new Satchels(world, chain, options);
  const shop = new Shop(world, chain, GAME, () => true, (xs) => xs[0]!, satchels);
  const broker = world.merchants.find((m) => m.id === "broker")!;
  return { world, chain, tokens, balances, calls, satchels, shop, broker, options, failOn: (m: string) => { fail = m; } };
}
const isStatus = (status: number) => (e: unknown) => e instanceof AuthError && e.status === status;
async function settled(chain: ChainWriter, id: string) {
  for (let i = 0; i < 100 && chain.shopReceipt(id)?.status === "pending"; i++) await delay(5);
  assert.notEqual(chain.shopReceipt(id)?.status, "pending");
  return chain.shopReceipt(id)!;
}

test("Satchel: five carried, extras in Case, ownership, isolation and restart persistence", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-satchel-"));
  t.after(() => { assert.ok(resolve(dir).startsWith(resolve(tmpdir()) + sep)); rmSync(dir, { recursive: true, force: true }); });
  const s = setup(join(dir, "satchels.json"));
  const first = await s.satchels.view(ACCOUNT);
  assert.equal(first.carried.length, 5); assert.equal(first.case.length, 3);
  assert.equal(first.carried[0], STARTER_TOKEN_ID);
  assert.deepEqual(first.supplies, { blank: 0, gilded: 0, salve: 0 });
  assert.deepEqual(first.keepsakes, []);
  await assert.rejects(s.satchels.carry(ACCOUNT, "tal-5", true), isStatus(409));
  await s.satchels.carry(ACCOUNT, STARTER_TOKEN_ID, false);
  await s.satchels.carry(ACCOUNT, "tal-5", true);
  await s.satchels.carry(ACCOUNT, "tal-5", true); // Idempotent, even at capacity.
  await assert.rejects(s.satchels.carry(OTHER, "tal-5", true), isStatus(403));
  s.satchels.addSupply(ACCOUNT, "blank", 3);
  const reloaded = new Satchels(s.world, s.chain, s.options);
  assert.deepEqual((await reloaded.view(ACCOUNT)).carried, (await s.satchels.view(ACCOUNT)).carried);
  assert.equal(reloaded.supplies(ACCOUNT).blank, 3);
  assert.equal(reloaded.supplies(OTHER).blank, 0);
  s.tokens[0]!.owner = OTHER;
  assert.ok(!(await reloaded.view(ACCOUNT)).carried.includes("tal-1"), "sold tokens leave the Satchel");
  await assert.rejects(reloaded.partner(ACCOUNT, "tal-1"), isStatus(403));
  await assert.rejects(reloaded.partner(ACCOUNT, "tal-6"), isStatus(403), "Case partners must be carried first");
});

test("Broker supplies: actual Ember payment, shared stock/quote, alias and no supply mint", async () => {
  const s = setup();
  for (const [itemId, supply] of [["blank-talisman", "blank"], ["gilded-talisman", "gilded"], ["ink-salve", "salve"], ["binding-seal", "blank"]] as const) {
    const canonical = itemId === "binding-seal" ? "blank-talisman" : itemId;
    const item = s.broker.items.find((i) => i.id === canonical)!;
    const price = quoteSellPrice(item, { wardGrudge: totalGrudge(s.world.wards.lantern.grudge), buyerIsOathbreaker: false }).price;
    const balance = s.balances[ACCOUNT]!, count = s.satchels.supplies(ACCOUNT)[supply];
    const answer = await s.shop.buy({ account: ACCOUNT, merchant: "broker", itemId });
    assert.equal(answer.status, 200, JSON.stringify(answer.body));
    const a = answer.body as TradeAnswer;
    assert.equal(a.price, price); assert.equal(a.itemId, canonical);
    assert.equal(s.satchels.supplies(ACCOUNT)[supply], count, "pending payment grants no supply");
    assert.equal((await settled(s.chain, a.jobId)).status, "done");
    assert.equal(s.satchels.supplies(ACCOUNT)[supply], count + 1);
    assert.equal(s.balances[ACCOUNT], balance - price);
    assert.equal(s.broker.items.find((i) => i.id === canonical)!.stock, item.stock - 1);
  }
  assert.ok(s.calls.every((m) => m === "mt_transfer"), "supply counts stay off chain");
});

test("Broker: failed payment restores stock/demand/purse and grants no supply", async () => {
  const s = setup(); s.failOn("mt_transfer");
  const item = s.broker.items.find((i) => i.id === "blank-talisman")!;
  const before = structuredClone(item), purse = s.broker.ember;
  const r = await s.shop.buy({ account: ACCOUNT, merchant: "broker", itemId: item.id });
  assert.equal((await settled(s.chain, (r.body as TradeAnswer).jobId)).status, "failed");
  assert.deepEqual(s.broker.items.find((i) => i.id === item.id), before);
  assert.equal(s.broker.ember, purse); assert.equal(s.satchels.supplies(ACCOUNT).blank, 0);
  assert.equal(s.shop.isTrading(ACCOUNT), false);
});

test("Broker: concurrent buyers cannot oversell the last blank", async () => {
  const s = setup(); s.broker.items.find((i) => i.id === "blank-talisman")!.stock = 1;
  const replies = await Promise.all([ACCOUNT, OTHER].map((account) => s.shop.buy({ account, merchant: "broker", itemId: "blank-talisman" })));
  assert.deepEqual(replies.map((r) => r.status).sort(), [200, 409]);
  const success = replies.find((r) => r.status === 200)!.body as TradeAnswer;
  await settled(s.chain, success.jobId);
  assert.equal(s.broker.items.find((i) => i.id === "blank-talisman")!.stock, 0);
  assert.equal(s.satchels.supplies(ACCOUNT).blank + s.satchels.supplies(OTHER).blank, 1);
});

test("An actual simulated agent purchase raises the player's supply quote", async () => {
  const s = setup(); const me = s.world.participants.find((p) => p.kind === "agent")!;
  me.ward = "lantern"; me.ember = 1000;
  const item = () => s.broker.items.find((i) => i.id === "blank-talisman")!;
  const quote = () => quoteSellPrice(item(), { wardGrudge: totalGrudge(s.world.wards.lantern.grudge), buyerIsOathbreaker: false }).price;
  const before = quote(), purse = me.ember;
  for (let i = 0; i < 8 && me.ember === purse; i++) step(s.world, createRng(i + 1), (_w, actor) => actor.id === me.id
    ? { type: "buy", merchantId: "broker", itemId: "blank-talisman" } : { type: "idle" });
  assert.ok(me.ember < purse, "the agent really bought from the world stock");
  assert.ok(quote() > before);
  const r = await s.shop.buy({ account: ACCOUNT, merchant: "broker", itemId: "blank-talisman" });
  assert.equal((r.body as TradeAnswer).price, quoteSellPrice({ ...item(), stock: item().stock + 1, buys24h: item().buys24h - 1 },
    { wardGrudge: totalGrudge(s.world.wards.lantern.grudge), buyerIsOathbreaker: false }).price);
  assert.ok((r.body as TradeAnswer).price > before);
  await settled(s.chain, (r.body as TradeAnswer).jobId);
});

test("Battle supplies: zero blank returns 409, accepted attempts consume exactly one of the chosen kind", async () => {
  const s = setup(); const partner = await s.satchels.partner(ACCOUNT);
  const battles = new BattleSessions(s.world, { satchels: s.satchels, newSeed: () => 99 });
  let start = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, partner).body as StartAnswer;
  const turn = (action: unknown) => battles.turn({ battleId: start.battleId, action }, ACCOUNT);
  assert.equal(start.view.canUse.seal, false); assert.equal(start.view.canUse.salve, false);
  const refused = turn({ kind: "seal" });
  assert.equal(refused.status, 409); assert.deepEqual(refused.body, { error: "No blank talismans: buy one from the Broker." });
  s.satchels.addSupply(ACCOUNT, "blank", 2); s.satchels.addSupply(ACCOUNT, "gilded");
  battles.abandonAccount(ACCOUNT);
  start = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, partner).body as StartAnswer;
  const plain = turn({ kind: "seal" }).body as TurnAnswer;
  assert.equal(plain.view.turn, 1, "refused moves did not spend a turn");
  assert.equal(s.satchels.supplies(ACCOUNT).blank, 1);
  const gold = turn({ kind: "seal", gilded: true }); assert.equal(gold.status, 200);
  assert.equal(s.satchels.supplies(ACCOUNT).gilded, 0); assert.equal(s.satchels.supplies(ACCOUNT).blank, 1);
  const repeat = turn({ kind: "seal", gilded: true }); assert.equal(repeat.status, 409);
  assert.equal(s.satchels.supplies(ACCOUNT).blank, 1);
});

async function boundBattle(s: ReturnType<typeof setup>, gilded: boolean, initialHp?: number, useSalve = false): Promise<TurnAnswer> {
  const supply: Supply = gilded ? "gilded" : "blank";
  for (let seed = 1; seed < 100; seed++) {
    await s.satchels.rest(ACCOUNT); s.satchels.addSupply(ACCOUNT, supply, 50);
    if (initialHp !== undefined) injure(s, initialHp);
    if (useSalve) s.satchels.addSupply(ACCOUNT, "salve");
    const battles = new BattleSessions(s.world, { satchels: s.satchels, newSeed: () => seed });
    const start = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, await s.satchels.partner(ACCOUNT)).body as StartAnswer;
    let view = start.view;
    if (useSalve) {
      const r = battles.turn({ battleId: start.battleId, action: { kind: "item", item: "salve" } }, ACCOUNT);
      assert.equal(r.status, 200); view = (r.body as TurnAnswer).view;
    }
    for (let i = 0; i < 100; i++) {
      const action = view.wild.hp > view.wild.maxHp * .6 ? { kind: "attack" } : { kind: "seal", gilded };
      const r = battles.turn({ battleId: start.battleId, action }, ACCOUNT); assert.equal(r.status, 200);
      const turn = r.body as TurnAnswer; view = turn.view;
      if (turn.outcome === "bound") return turn;
      if (turn.outcome) break;
    }
  }
  throw new Error("No test seed produced a binding");
}
function record(t: TurnAnswer): BattleRecord {
  assert.ok(t.outcome === "bound" || t.outcome === "won");
  return { battleId: t.battleId, account: ACCOUNT, outcome: t.outcome, points: t.points!, seed: t.seed!, actions: t.actions!,
    wild: { speciesId: t.view.wild.speciesId, emotion: t.view.wild.emotion, omen: t.view.wild.omen },
    playerCurse: t.view.player.speciesId, initial: t.initial };
}
test("A failed mint refunds the chosen binding supply once, including after store restart", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-satchel-refund-"));
  t.after(() => { assert.ok(resolve(dir).startsWith(resolve(tmpdir()) + sep)); rmSync(dir, { recursive: true, force: true }); });
  for (const gilded of [false, true]) {
    const s = setup(join(dir, `${gilded}.json`)); s.failOn("mint_talisman");
    const end = await boundBattle(s, gilded), supply = gilded ? "gilded" : "blank";
    assert.equal(s.satchels.isBinding(ACCOUNT), true);
    const before = s.satchels.supplies(ACCOUNT)[supply];
    const reloaded = new Satchels(s.world, s.chain, s.options);
    const receipt = await s.chain.record(record(end), 1000, (r) => reloaded.settleBinding(ACCOUNT, end.battleId, r));
    assert.equal(receipt.status, "failed"); assert.equal(reloaded.supplies(ACCOUNT)[supply], before + 1);
    reloaded.settleBinding(ACCOUNT, end.battleId, receipt);
    assert.equal(reloaded.supplies(ACCOUNT)[supply], before + 1, "polling/repeated settlement cannot refund twice");
  }
});

test("A successful mint followed by failed points does not refund a second talisman", async () => {
  const s = setup(); s.failOn("submit_battle"); const end = await boundBattle(s, false);
  assert.equal(s.satchels.isBinding(ACCOUNT), true);
  const before = s.satchels.supplies(ACCOUNT).blank;
  const r = await s.chain.record(record(end), 1000, (receipt) => s.satchels.settleBinding(ACCOUNT, end.battleId, receipt));
  assert.equal(r.status, "failed"); assert.equal(r.tokenId, "minted-token");
  assert.equal(s.satchels.supplies(ACCOUNT).blank, before);
  assert.equal(s.satchels.isBinding(ACCOUNT), false);
});

test("Ink persists across battles and restart; zero ink blocks another fight; rest restores without a world tick", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "gb-satchel-ink-"));
  t.after(() => { assert.ok(resolve(dir).startsWith(resolve(tmpdir()) + sep)); rmSync(dir, { recursive: true, force: true }); });
  const s = setup(join(dir, "ink.json"));
  const battles = new BattleSessions(s.world, { satchels: s.satchels, newSeed: () => 99 });
  const start = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, await s.satchels.partner(ACCOUNT)).body as StartAnswer;
  assert.equal(battles.start({ ward: "lantern", account: ACCOUNT }, undefined, await s.satchels.partner(ACCOUNT)).status, 409);
  let turn: TurnAnswer;
  do {
    const r = battles.turn({ battleId: start.battleId, action: { kind: "ward" } }, ACCOUNT); assert.equal(r.status, 200);
    turn = r.body as TurnAnswer;
  } while (!turn.outcome);
  assert.equal(turn.view.player.hp, 0);
  const reloaded = new Satchels(s.world, s.chain, s.options);
  assert.equal((await reloaded.view(ACCOUNT)).ink.starter!.hp, 0);
  await assert.rejects(reloaded.partner(ACCOUNT), (e) => e instanceof AuthError && e.status === 409 && e.message === "Visit the Mender or rest");
  s.world.tick = 21; const clock = s.world.tick;
  const rested = await reloaded.rest(ACCOUNT);
  assert.equal(s.world.tick, clock); assert.equal(rested.personalTick, 30); assert.equal(rested.hour, 6);
  const next = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, await reloaded.partner(ACCOUNT)).body as StartAnswer;
  assert.equal(next.view.player.hp, next.view.player.maxHp);
  const restAgain = await reloaded.rest(ACCOUNT); assert.equal(restAgain.personalTick, 54);
  assert.equal(s.world.tick, clock);
});

function injure(s: ReturnType<typeof setup>, hp: number, tokenId = STARTER_TOKEN_ID) {
  const state = createBattle({ wild: "hushmoth", playerCurse: DEFAULT_PLAYER_CURSE }); state.player.hp = hp;
  s.satchels.applyTurn(ACCOUNT, tokenId, "injury", { kind: "ward" }, state);
}
test("A second fight starts from the previous fight's ink, even if that fight was abandoned", async () => {
  const s = setup(); injure(s, 20);
  const battles = new BattleSessions(s.world, { satchels: s.satchels, newSeed: () => 99 });
  const first = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, await s.satchels.partner(ACCOUNT)).body as StartAnswer;
  assert.equal(first.view.player.hp, 20);
  const r = battles.turn({ battleId: first.battleId, action: { kind: "ward" } }, ACCOUNT).body as TurnAnswer;
  battles.abandonAccount(ACCOUNT);
  assert.ok(r.view.player.hp > 0);
  const next = battles.start({ ward: "lantern", account: ACCOUNT }, undefined, await s.satchels.partner(ACCOUNT)).body as StartAnswer;
  assert.equal(next.view.player.hp, r.view.player.hp);
});

test("Mender: price follows Sorrow, paid restoration, failed payment leaves ink unchanged", async () => {
  const s = setup(); injure(s, 0);
  assert.equal(menderQuote(s.world).price, Math.round(8 * (1 + s.world.wards.lantern.grudge.sorrow / 100)));
  s.world.wards.lantern.grudge.sorrow = 100;
  assert.equal(menderQuote(s.world).price, 16); assert.match(menderQuote(s.world).reason, /Sorrow is high in Lantern Ward/);
  const before = s.balances[ACCOUNT]!;
  const paid = await s.satchels.restore(ACCOUNT, STARTER_TOKEN_ID);
  assert.equal(s.satchels.isRestoring(ACCOUNT), true);
  await assert.rejects(s.satchels.rest(ACCOUNT), isStatus(409));
  assert.equal((await settled(s.chain, paid.jobId)).status, "done");
  assert.equal(s.balances[ACCOUNT], before - 16);
  assert.equal((await s.satchels.partner(ACCOUNT)).hp, (await s.satchels.partner(ACCOUNT)).maxHp);
  injure(s, 5); s.failOn("mt_transfer");
  const failed = await s.satchels.restore(ACCOUNT, STARTER_TOKEN_ID);
  await settled(s.chain, failed.jobId);
  assert.equal((await s.satchels.partner(ACCOUNT)).hp, 5);
  assert.equal(s.satchels.isRestoring(ACCOUNT), false);
});

test("Salves outside battle consume one, heal partially and reject full ink without spending", async () => {
  const s = setup(); s.satchels.addSupply(ACCOUNT, "salve", 2);
  await assert.rejects(s.satchels.useSalve(ACCOUNT, STARTER_TOKEN_ID), isStatus(409));
  assert.equal(s.satchels.supplies(ACCOUNT).salve, 2);
  injure(s, 1);
  const r = await s.satchels.useSalve(ACCOUNT, STARTER_TOKEN_ID);
  assert.equal(r.ink.hp, 1 + Math.round(r.ink.maxHp * .4)); assert.equal(r.supplies.salve, 1);
});

test("Damaged partner and finite supplies replay exactly; initial ink is covered by the proof hash", async () => {
  const s = setup(); const end = await boundBattle(s, false, 30, true);
  assert.equal(end.initial!.playerHp, 30); assert.deepEqual(end.actions![0], { kind: "item", item: "salve" });
  const replay = record(end), logHash = battleLogHash(replay);
  const result = checkBattle({ ...replay, logHash, ward: "lantern", at: 0 });
  assert.equal(result.outcomeMatches, true); assert.equal(result.hashMatches, true);
  assert.notEqual(battleLogHash({ ...replay, initial: { ...replay.initial!, playerHp: 1 } }), logHash);
});

test("HTTP Satchel routes enforce sessions, reject account injection and keep rest local", { timeout: 30_000 }, async (t) => {
  const reservation = createServer(); await new Promise<void>((r) => reservation.listen(0, "127.0.0.1", r));
  const port = (reservation.address() as { port: number }).port; await new Promise<void>((r) => reservation.close(() => r()));
  const dir = mkdtempSync(join(tmpdir(), "gb-satchel-http-"));
  const child = spawn(process.execPath, ["--import", "tsx", "server/src/index.ts"], { windowsHide: true, env: { ...process.env,
    PORT: String(port), HOST: "127.0.0.1", NODE_ENV: "test", TICK_MS: "3600000", NEAR_CHAIN: "off", NEAR_AI_API_KEY: "",
    AUTH_EMAIL_MODE: "console", AUTH_SECRET: randomBytes(32).toString("hex"), AUTH_STORE_PATH: join(dir, "auth.enc"),
    SATCHEL_STORE_PATH: join(dir, "satchels.json"), BATTLE_ARCHIVE: join(dir, "battles.jsonl"), AUTH_SIGN_IN_URL: "http://localhost:3000/sign-in" }, stdio: ["ignore", "pipe", "pipe"] });
  t.after(async () => {
    if (child.exitCode === null) { const closed = new Promise((r) => child.once("exit", r)); child.kill(); await closed; }
    assert.ok(resolve(dir).startsWith(resolve(tmpdir()) + sep)); rmSync(dir, { recursive: true, force: true });
  });
  let output = "", errors = ""; child.stdout.on("data", (b) => { output += b; }); child.stderr.on("data", (b) => { errors += b; });
  const base = `http://127.0.0.1:${port}`;
  const post = (path: string, body: unknown, headers = {}) => fetch(base + path, { method: "POST", headers: { "Content-Type": "application/json", ...headers }, body: JSON.stringify(body) });
  for (let i = 0; i < 150 && !output.includes("Grudgebound server on"); i++) { assert.equal(child.exitCode, null, errors); await delay(50); }
  assert.ok(output.includes("Grudgebound server on"), errors);
  assert.equal((await fetch(base + "/satchel")).status, 401);
  for (const path of ["/profile/complete", "/satchel/carry", "/satchel/salve", "/satchel/equip", "/exploration/interact", "/mender/restore", "/rest"]) assert.equal((await post(path, {})).status, 401);
  const start = await post("/auth/email/start", { email: "satchel@example.com" });
  const { challengeId } = await start.json() as { challengeId: string };
  for (let i = 0; i < 50 && !output.includes("[local sign-in]"); i++) await delay(20);
  const code = /satchel@example.com: (\d{6})/.exec(output)![1];
  const login = await post("/auth/email/verify", { challengeId, code }); assert.equal(login.status, 200);
  const headers = { Cookie: login.headers.get("set-cookie")!.split(";")[0]!, Origin: "http://localhost:3000" };
  const satchel = await fetch(base + "/satchel", { headers }); assert.equal(satchel.status, 200);
  // A brand-new player gets Kessa's starter kit, so the first bind needs no Ember.
  assert.deepEqual((await satchel.json() as { supplies: object }).supplies, { blank: 3, gilded: 0, salve: 1 });
  assert.equal((await fetch(base + "/satchel?account=victim.testnet", { headers })).status, 403);
  assert.equal((await post("/rest", { account: "victim.testnet" }, headers)).status, 403);
  const arrival = { name: "Ari", look: "feminine", pronouns: "she", starter: "hushmoth" };
  assert.equal((await post("/profile/complete", { ...arrival, account: "victim.testnet" }, headers)).status, 403);
  assert.equal((await post("/profile/complete", { ...arrival, name: "<bad>" }, headers)).status, 400);
  assert.equal((await post("/profile/complete", arrival, headers)).status, 200);
  assert.equal((await post("/profile/complete", { ...arrival, starter: "weepwick" }, headers)).status, 409);
  assert.equal((await post("/satchel/carry", { tokenId: "foreign", carry: true }, headers)).status, 403);
  assert.equal((await post("/exploration/interact", { id: "alley-parcel", account: "victim.testnet" }, headers)).status, 403);
  assert.equal((await post("/exploration/interact", { id: "storehouse-chest" }, headers)).status, 403);
  for (let i = 0; i < 2; ++i) {
    const found = await post("/exploration/interact", { id: "alley-parcel", blank: 99999 }, headers);
    assert.equal(found.status, 200);
    assert.deepEqual((await found.json() as { supplies: object }).supplies, { blank: 5, gilded: 0, salve: 2 });
  }
  assert.equal((await post("/satchel/equip", { itemId: "etched-focus" }, headers)).status, 403);
  const before = await (await fetch(base + "/state")).json() as { tick: number };
  const rest = await post("/rest", {}, headers); assert.equal(rest.status, 200);
  assert.equal((await (await fetch(base + "/state")).json() as { tick: number }).tick, before.tick);
  const battle = await post("/battle/start", { ward: "lantern" }, headers); assert.equal(battle.status, 200);
  const { battleId } = await battle.json() as StartAnswer;
  assert.equal((await post("/rest", {}, headers)).status, 409);
  assert.equal((await post("/profile/complete", arrival, headers)).status, 409);
  assert.equal((await post("/satchel/equip", { itemId: "" }, headers)).status, 409);
  assert.equal((await post("/exploration/interact", { id: "lamp-kit" }, headers)).status, 409);
  // With the starter kit a brand-new player can attempt a seal straight away (no Ember needed).
  const firstSeal = await post("/battle/turn", { battleId, action: { kind: "seal" } }, headers);
  assert.equal(firstSeal.status, 200);
});

test("the starter kit is granted once per account, on top of anything bought first, and survives a reload", async () => {
  const dir = mkdtempSync(join(tmpdir(), "gb-kit-"));
  try {
    const file = join(dir, "satchels.json");
    const s = setup();
    const kit = new Satchels(s.world, s.chain, { file, starterKit: STARTER_KIT });
    kit.addSupply(ACCOUNT, "blank"); // bought before the Satchel was ever opened
    assert.deepEqual((await kit.view(ACCOUNT)).supplies, { blank: 4, gilded: 0, salve: 1 });
    assert.deepEqual((await kit.view(ACCOUNT)).supplies, { blank: 4, gilded: 0, salve: 1 });
    const reloaded = new Satchels(s.world, s.chain, { file, starterKit: STARTER_KIT });
    assert.deepEqual((await reloaded.view(ACCOUNT)).supplies, { blank: 4, gilded: 0, salve: 1 });
    assert.deepEqual((await reloaded.view(OTHER)).supplies, { blank: 3, gilded: 0, salve: 1 });
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

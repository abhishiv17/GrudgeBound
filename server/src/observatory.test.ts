import { test } from "node:test";
import assert from "node:assert/strict";
import { createWorld, createRng, dayOf, step, enactRule, repealRule, log } from "@grudgebound/shared";
import { Observatory, observatoryPreview } from "./observatory";

test("Observatory preview is deterministic, isolated and includes the demo milestones", () => {
  const world = createWorld(2), rng = createRng(2), control = createRng(2);
  const before = structuredClone(world);
  const preview = observatoryPreview();
  assert.deepEqual(observatoryPreview(), preview);
  assert.deepEqual(world, before);
  assert.equal(rng.next(), control.next());
  assert.equal(preview.samples.length, 168);
  assert.equal(preview.mode, "preview");
  assert.ok(preview.events.some((e) => e.type === "bankruptcy" && dayOf(e.tick) === 3));
  assert.ok(preview.events.some((e) => e.type === "calamity" && dayOf(e.tick) === 4));
  assert.ok(preview.rules.some((r) => r.authorKind === "agent"));
  assert.ok(preview.rules.some((r) => r.status === "rejected" && r.reason));
  preview.samples.length = 0;
  assert.equal(observatoryPreview().samples.length, 168, "callers cannot mutate cached history");
});

test("Observatory live ring caps at 168 detached samples, including the latest tick", () => {
  const world = createWorld(2), rng = createRng(2), history = new Observatory(world);
  assert.deepEqual(history.view().samples, []);
  for (let i = 0; i < 180; i++) { step(world, rng); history.sample(); }
  const view = history.view();
  assert.equal(view.samples.length, 168);
  assert.equal(view.samples[0]!.tick, 13);
  assert.equal(view.samples.at(-1)!.tick, 180);
  const price = view.samples[0]!.prices.broker!["talisman-rage"];
  world.merchants[0]!.items[0]!.base = 999;
  assert.equal(history.view().samples[0]!.prices.broker!["talisman-rage"], price);
  view.samples[0]!.prices.broker!["talisman-rage"] = -1;
  assert.equal(history.view().samples[0]!.prices.broker!["talisman-rage"], price);
  assert.ok(view.events.every((e) => ["bankruptcy", "calamity", "rule-added", "rule-rejected", "rule-repealed"].includes(e.type)));
});

test("Observatory retains rule authors, rejection reasons and repeals in newest-first order", () => {
  const world = createWorld(2), history = new Observatory(world);
  const rule = enactRule(world, { author: "fen", authorName: "Old Mother Fen", templateId: "R8", params: { ward: "lantern" }, cost: 50 });
  history.sample();
  world.tick = 3;
  repealRule(world, rule.id, { by: "alice.gb.testnet", byName: "Alice", cost: 100 });
  log(world, { type: "rule-rejected", actor: "agent-rook-ash.gb.testnet", text: 'Kessa rejects Rook & Ash\'s rule "A rule": Contradicts another rule.', url: "https://testnet.nearblocks.io/txns/proof" });
  const view = history.view();
  assert.deepEqual(view.rules.map((r) => r.status), ["repealed", "rejected"]);
  assert.equal(view.rules[0]!.author, "Old Mother Fen");
  assert.equal(view.rules[1]!.author, "Rook & Ash");
  assert.equal(view.rules[1]!.reason, "Contradicts another rule.");
  assert.match(view.rules[1]!.url!, /proof$/);
  assert.equal(history.view().rules.length, 2, "events are collected once");
});

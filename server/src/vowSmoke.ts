/** Verify real stake locking and kept/broken settlement with the existing testnet demo player.
 * No new accounts, email, keys, or balances are created. This deliberately stakes points on testnet.
 */
import { setTimeout as delay } from "node:timers/promises";
import { createWorld } from "@grudgebound/shared";
import { ChainWriter, chainConfigFromEnv, custodialKey } from "./chain";
import { Vows } from "./vows";

const cfg = chainConfigFromEnv();
if (!cfg || cfg.network !== "testnet") throw new Error("Vow verification requires the existing NEAR testnet configuration.");
const account = process.argv[2] || cfg.guestAccount;
if (!custodialKey(cfg, account)) throw new Error("The existing testnet demo player's signer is unavailable.");
const chain = new ChainWriter(cfg);
const vows = new Vows(createWorld(2), chain, (a) => a === account, 1000);

async function receipt(id: string) {
  for (let i = 0; i < 180; i++) {
    const r = chain.vowReceipt(id);
    if (r && r.status !== "pending") {
      if (r.status !== "done") throw new Error(r.error || "The Rite refused the vow");
      for (const tx of r.txs) console.log(`${tx.kind}: ${tx.url}`);
      return r;
    }
    await delay(500);
  }
  throw new Error("Verification timed out; inspect the ledger before retrying.");
}

const initialPoints = await chain.points(account);
const previous = await vows.snapshot(account);
if (previous.some((v) => v.status === "Active" && v.template_id === "V2")) throw new Error("The demo player already has an active V2. Let it settle before verifying.");
if (initialPoints < 30) throw new Error("The existing demo player needs at least 30 earned points for this check. No points were added.");
console.log(`Vow testnet verification: ${account}, ${initialPoints} points. One town day = 24 seconds.`);

const made = await receipt((await vows.swear(account, { templateId: "V2", params: {}, days: 1 })).jobId);
if (await chain.points(account) !== initialPoints - 15) throw new Error("The first stake was not locked.");
for (let i = 0; i < 180; i++) {
  const vow = (await vows.snapshot(account)).find((v) => v.id === made.vowId);
  if (vow?.status === "Kept") break;
  await vows.sweep([account]);
  await delay(500);
}
if ((await vows.snapshot(account)).find((v) => v.id === made.vowId)?.status !== "Kept") throw new Error("The kept vow did not settle.");
if (await chain.points(account) !== initialPoints + 3) throw new Error("The kept vow did not return stake + 20%.");
console.log("PASS: stake locked, deadline survived re-reads, and kept vow returned 18 points.");

const broken = await receipt((await vows.swear(account, { templateId: "V2", params: {}, days: 1 })).jobId);
if (broken.vowId === undefined) throw new Error("No vow id returned");
await receipt((await vows.release(account, broken.vowId)).jobId);
if ((await vows.snapshot(account)).find((v) => v.id === broken.vowId)?.status !== "Broken") throw new Error("The released vow was not marked broken");
if (await chain.points(account) !== initialPoints - 12) throw new Error("The released vow's stake was not burned");
console.log("PASS: early release burned its 15-point stake. Verification complete.");

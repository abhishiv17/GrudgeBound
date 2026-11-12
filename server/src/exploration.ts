/** Sealed Storehouse: account-owned progress; mutations occur atomically inside the Satchel store.
 * Dialogue/record text is a draft for owner review. No world economy events are fabricated here.
 */
import { GEAR, equipmentSnapshot } from "@grudgebound/shared";
import { AuthError } from "./auth";
import { RIVAL_FLAGS, tideOpen } from "./rival";

export interface Exploration { flags: string[]; gear: string[]; equipped: string; tracked: string }
export const newExploration = (): Exploration => ({ flags: [], gear: [], equipped: "", tracked: "rival" });
const TASKS = ["rival", "lamp", "net", "storehouse"];
/**
 * First-time tips (DRAFT text), each shown once per account and then kept in the Journal. The client decides when a
 * moment has come (first walk, first battle, ...) and reports it to /exploration/tip; the server only remembers it.
 */
export const TIPS = [
  { id: "beacon", text: "The gold beacon marks the next step of the task you track. Choose a different task in Satchel > Journal, or turn the beacon off in Settings." },
  { id: "battle", text: "Attack to wear a curse down, then seal it with a blank talisman while it is weak. Wins and binds teach your partner; fleeing and losing teach nothing." },
  { id: "find", text: "Finds go into your Satchel: supplies, Gear charms and Keepsakes. Keepsakes often hold the next clue." },
  { id: "level", text: "Your partner grew a level: more ink, more strength. Its experience is shown in the Satchel." },
  { id: "tide", text: "Tide Ward. Its curses are stronger, levels 3 to 7. The Net mender works the docks; a sealed storehouse stands on the north quay." },
];
const FLAGS = ["alley-parcel", "lamp-kit", "delivery-slip", "reed-bundle", "dock-tin", "door-seen",
  "lamp-accepted", "lamp-done", "net-accepted", "tide-cleared", "tide-bound", "net-done", "clerk-done", "storehouse-open", "storehouse-chest", ...RIVAL_FLAGS,
  ...TIPS.map(t => `tip-${t.id}`)];
export function validExploration(e: Exploration): boolean {
  return !!e && Array.isArray(e.flags) && e.flags.every(f => FLAGS.includes(f)) && new Set(e.flags).size === e.flags.length
    && Array.isArray(e.gear) && e.gear.every(id => !!equipmentSnapshot(id)) && new Set(e.gear).size === e.gear.length
    && (e.equipped === "" || e.gear.includes(e.equipped)) && TASKS.includes(e.tracked);
}
interface RewardPocket { exploration: Exploration; keepsakes: string[]; supplies: { blank: number; gilded: number; salve: number }; profile?: Who }
/** Who the townsfolk are talking to or about: the arrival profile's name and pronouns. */
export interface Who { name: string; pronouns: string }
const PRONOUNS: Record<string, [string, string, string]> = { he: ["he", "him", "his"], she: ["she", "her", "her"], they: ["they", "them", "their"] };
function words(who?: Who) {
  const [they, them, their] = PRONOUNS[who?.pronouns ?? ""] ?? PRONOUNS.they!;
  const name = who?.name || "the new exorcist";
  return { name, Name: name[0]!.toUpperCase() + name.slice(1), they, them, their };
}
/** Flags are kept in the order they happened: true when `first` happened and `then` did not, or happened later. */
const earlier = (e: Exploration, first: string, then: string) => {
  const i = e.flags.indexOf(first), j = e.flags.indexOf(then);
  return i >= 0 && (j < 0 || i < j);
};

/**
 * What the streets say about the player's own deeds, newest first (DRAFT lines). Townsfolk mix these into their talk.
 * Only facts the server recorded for this account are used, so nobody gossips about something that did not happen.
 */
export function gossip(e: Exploration, who?: Who): string[] {
  const has = (f: string) => e.flags.includes(f);
  const { name, Name, they, them, their } = words(who);
  const out: string[] = [];
  if (has("rival-3-won")) out.push(`Dace Marrow lost to ${name} three times and still talks to ${them}. Now the two of them pester the Rite Hall together.`);
  if (has("storehouse-chest")) out.push(`Someone opened the sealed storehouse in Tide. People say it was ${name}, and that ${they} came out with a ledger leaf.`);
  if (has("net-done")) out.push(has("tide-bound")
    ? `${Name} bound a curse out of Tide's reeds. The Net mender says it is the first quiet night in a month.`
    : `${Name} beat a curse in Tide's reeds but left it loose. The Net mender is still counting the knots.`);
  if (has("rival-2-won")) out.push(`The ferryman will take ${name} across now. Dace Marrow lost at the southern pier and went quiet for once.`);
  if (has("lamp-done")) out.push(earlier(e, "lamp-kit", "lamp-accepted")
    ? `The Lamplighter's kit came back before anyone asked. ${Name} found it and walked it over.`
    : `The Lamplighter asked ${name} to find the kit, and ${they} did. Every wick on the north lane is trimmed again.`);
  if (has("rival-1-won") && !has("rival-2-won")) out.push(`${Name} beat Dace Marrow on the pier, first day off the boat. Dace tells everyone it was luck.`);
  if (has("rival-1-lost") && !has("rival-2-won")) out.push(`Dace Marrow beat ${name} on the pier and tells everyone. Don't let ${them} hear you mention it.`);
  if (out.length === 0) out.push(`There's a new exorcist in town. ${Name}, I think. Nobody knows ${their} business yet.`);
  return out;
}
const NOTES = [
  { id: "lamp-kit", title: "Lamp kit", text: "A bundle of spare wicks and tools. The Lamplighter walks the north market lane in Lantern Ward." },
  { id: "delivery-slip", title: "Marked delivery slip", text: "A dockside storehouse stamp marks this delivery. Ask the Receipt clerk after finding its key." },
  { id: "reed-bundle", title: "Reed-edge bundle", text: "A spool of netting recovered from Tide's eastern reeds. The Net mender works along the docks." },
  { id: "storehouse-key", title: "Brass storehouse key", text: "The bow bears the same mark as the delivery slip. The Receipt clerk may recognise it." },
  { id: "storehouse-record", title: "A copied ledger leaf", text: "The same stamped symbol appears on this storehouse record and your debt notice. A line beneath it reads: Copy retained; original forwarded to the Rite Hall. Who kept the original?" },
];
export function explorationView(e: Exploration, keepsakes: string[], who?: Who) {
  const has = (flag: string) => e.flags.includes(flag);
  const rivalStatus = has("rival-3-won") ? "complete" : "active";
  // Where the next step happens, for the client's waypoint: "find:<id>" (a find or door), "npc:<role>", "rival",
  // "ward:<id>" (anywhere in that Ward), or "" when there is nothing to walk to.
  const storehouseTarget = has("storehouse-chest") ? "" : has("clerk-done") ? (has("storehouse-open") ? "find:storehouse-chest" : "find:storehouse-door")
    : !has("delivery-slip") ? (has("lamp-kit") ? "npc:lamplighter" : "find:lamp-kit") : !has("net-done") ? netTarget(e) : "npc:receipt-clerk";
  const rivalTarget = has("rival-3-won") ? "" : has("rival-2-won") && !has("storehouse-chest") ? storehouseTarget : "rival";
  const journal = [
    { id: "rival", title: "A rival on the pier", status: rivalStatus,
      objective: has("rival-3-won") ? "You beat Dace Marrow three times. The Rite Hall holds the original record."
        : !has("rival-1-won") && !has("rival-1-lost") ? "Face Dace Marrow on the harbour pier."
        : !has("rival-2-won") ? "Train your partner to level 4, then beat Dace at Lantern's southern pier to earn the ferry."
        : !has("storehouse-chest") ? "Open the sealed storehouse in Tide; Dace is waiting on the quay."
        : "Beat Dace on Tide's quay. A level 7 partner and a good charm will help.",
      hint: "Partners grow from won and bound battles. Fleeing or losing teaches nothing. Rest or visit the Mender before a rival fight.", target: rivalTarget },
    { id: "lamp", title: "A light returned", status: has("lamp-done") ? "complete" : has("lamp-accepted") || has("lamp-kit") ? "active" : "available",
      objective: has("lamp-done") ? "The Lamplighter's kit is back. Your Worn Focus is in Gear." : has("lamp-kit") ? "Return the kit to the Lamplighter in Lantern Ward." : "Ask the Lamplighter about the missing kit.",
      hint: "Inspect the lamp kit beside the western service lane, away from the main stalls.",
      target: has("lamp-done") ? "" : has("lamp-kit") || !has("lamp-accepted") ? "npc:lamplighter" : "find:lamp-kit" },
    { id: "net", title: "Among the reeds", status: has("net-done") ? "complete" : has("net-accepted") ? "active" : "available",
      objective: has("net-done") ? "The Net mender gave you a Ward Clasp and a brass key." : !has("net-accepted") ? "Speak with the Net mender on Tide's docks."
        : !has("reed-bundle") ? "Recover the bundle by Tide's eastern reeds." : !has("tide-cleared") ? "Win or bind one curse in Tide after accepting the request." : "Return to the Net mender with the bundle.",
      hint: "Take the ferry from Lantern Ward's southern pier. Check the reed edge in Tide; a win counts without spending a blank.", target: has("net-done") ? "" : netTarget(e) },
    { id: "storehouse", title: "The sealed storehouse", status: has("storehouse-chest") ? "complete" : has("delivery-slip") || has("door-seen") ? "active" : "available",
      objective: has("storehouse-chest") ? "You found an Etched Focus, a gilded talisman and a copied ledger leaf." : !has("clerk-done") ? "Bring the marked slip and brass key to the Receipt clerk in Lantern Ward." : "Open the marked storehouse on Tide's quay and inspect its chest.",
      hint: "The door sits along Tide's northern quay. The slip and key are kept when used.", target: storehouseTarget },
  ];
  return { version: 1, flags: e.flags, equipped: e.equipped, tracked: e.tracked,
    gear: GEAR.filter(g => e.gear.includes(g.id)), journal, notes: NOTES.filter(n => keepsakes.includes(n.id)), gossip: gossip(e, who),
    tips: TIPS.map(t => ({ ...t, seen: e.flags.includes(`tip-${t.id}`) })) };
}
/** Remembers that a tip was shown. Allowed mid-battle and beside other actions: it changes nothing else. */
export function seeTip(e: Exploration, id: unknown): void {
  if (typeof id !== "string" || !TIPS.some(t => t.id === id)) throw new AuthError(400, "Unknown tip.");
  if (!e.flags.includes(`tip-${id}`)) e.flags.push(`tip-${id}`);
}
function netTarget(e: Exploration): string {
  const has = (f: string) => e.flags.includes(f);
  return !has("net-accepted") ? "npc:net-mender" : !has("reed-bundle") ? "find:reed-bundle" : !has("tide-cleared") ? "ward:tide" : "npc:net-mender";
}

export function interact(p: RewardPocket, id: unknown): string {
  if (typeof id !== "string") throw new AuthError(400, "Choose an interaction.");
  const e = p.exploration;
  const has = (f: string) => e.flags.includes(f);
  const mark = (f: string) => { if (!has(f)) e.flags.push(f); };
  const note = (n: string) => { if (!p.keepsakes.includes(n)) p.keepsakes.push(n); };
  const gear = (g: string) => { if (!e.gear.includes(g)) e.gear.push(g); };
  const { name, Name } = words(p.profile);
  const supplies = (key: keyof RewardPocket["supplies"], count: number) => {
    if (!Number.isSafeInteger(p.supplies[key] + count)) throw new AuthError(409, "Your supplies are full.");
    p.supplies[key] += count;
  };
  if (["reed-bundle", "dock-tin", "net-mender", "storehouse-door", "storehouse-chest"].includes(id) && !tideOpen(e))
    throw new AuthError(403, "The ferryman won't take you to Tide until you beat Dace Marrow at the southern pier.");
  switch (id) {
    case "alley-parcel":
      if (has(id)) return "The parcel is empty. You already packed its supplies.";
      mark(id); supplies("blank", 2); supplies("salve", 1); return "Found: 2 blank talismans and 1 Ink Salve. Open your Satchel to see them.";
    case "lamp-kit":
      if (has(id)) return "You already found the lamp kit. Check your Journal.";
      mark(id); note(id); return "Found the lamp kit. The Lamplighter will want this back.";
    case "delivery-slip":
      if (has(id)) return "The marked delivery slip is in Keepsakes.";
      mark(id); note(id); return "Found a marked delivery slip. Its stamp points toward Tide's storehouse.";
    case "reed-bundle":
      if (has(id)) return "The reed-edge bundle is already in your Keepsakes.";
      mark(id); note(id); return "Recovered a bundle of netting. Find the Net mender on Tide's docks.";
    case "dock-tin":
      if (has(id)) return "The supply tin is empty.";
      mark(id); supplies("salve", 1); return "Found: 1 Ink Salve.";
    case "lamplighter":
      if (has("lamp-done")) {
        if (has("storehouse-chest")) return `Lamplighter: That slip in my kit led you to a whole storehouse? I only trim wicks, ${name}. Still, I'm glad it was you who found it.`;
        if (has("rival-2-won")) return `Lamplighter: I watched you beat that Dace boy from the end of the lane. The focus suits you.`;
        return earlier(e, "lamp-kit", "lamp-accepted")
          ? `Lamplighter: You brought my kit back before I'd even asked, ${name}. I still owe you for that.`
          : `Lamplighter: My kit is back where it belongs. Keep that focus close, ${name}.`;
      }
      const unasked = !has("lamp-accepted");
      mark("lamp-accepted"); e.tracked = "lamp";
      if (!has("lamp-kit")) return has("rival-1-lost")
        ? "Lamplighter: Lost to Dace on the pier? Then you'll want a focus. My kit went missing by the western service lane. Bring it back and this one is yours."
        : "Lamplighter: My kit went missing by the western service lane. Bring it back and this focus is yours.";
      mark("lamp-done"); gear("worn-focus"); mark("delivery-slip"); note("delivery-slip");
      return (unasked ? `Lamplighter: My kit! I hadn't even asked yet, ${name}. ` : "Lamplighter: You found it. ")
        + "Received: Worn Focus (+1 Attack). A delivery slip in the kit bears a Tide storehouse stamp. Equip your charm in Satchel > Gear.";
    case "net-mender":
      if (has("net-done")) {
        if (has("storehouse-chest")) return `Net mender: So my old key opened the sealed storehouse. Copies of everyone's debts, they say. Mind who you show that leaf to, ${name}.`;
        return has("tide-bound")
          ? `Net mender: The reeds have been quiet since you bound that curse. The brass key is yours, ${name}.`
          : "Net mender: You beat it, but you let it go. It'll be back in the reeds by the next tide. The key is yours all the same.";
      }
      const brought = has("reed-bundle") && !has("net-accepted");
      mark("net-accepted"); e.tracked = "net";
      if (!has("reed-bundle")) return "Net mender: Recover my bundle by the eastern reeds, and win or bind a curse here in Tide. I will make it worth your while.";
      if (!has("tide-cleared")) return brought
        ? "Net mender: You already found my bundle? Then do one more thing: win or bind a Tide curse before coming back. Fleeing will not do."
        : "Net mender: You found it. Now win or bind a Tide curse before coming back; fleeing will not do.";
      mark("net-done"); gear("ward-clasp"); note("storehouse-key");
      return (has("tide-bound") ? "Net mender: Bound it, too. That one won't trouble my nets again. " : "Net mender: Beaten, if not bound. It'll do. ")
        + "Received: Ward Clasp (+2 Guard) and a brass storehouse key. Ask the Receipt clerk about its mark.";
    case "receipt-clerk":
      e.tracked = "storehouse";
      if (has("storehouse-chest")) return `Receipt clerk: "Copy retained; original forwarded to the Rite Hall." I've stamped that line a thousand times, ${name}. I never asked who reads the originals.`;
      if (has("clerk-done")) return "Receipt clerk: The marked storehouse stands on Tide's northern quay. Your key fits its door.";
      if (!has("delivery-slip") || !has("net-done")) return "Receipt clerk: Bring me the marked delivery slip and its matching brass key. Then I can trace the address.";
      mark("clerk-done"); return "Lead discovered: the sealed storehouse on Tide's northern quay. The marks match; take your key to its door.";
    case "storehouse-door":
      mark("door-seen"); e.tracked = "storehouse";
      if (!has("clerk-done")) return "A marked lock holds the door. Find its key and ask the Receipt clerk about the matching delivery slip.";
      mark("storehouse-open"); return `The brass key turns. ${Name}, the storehouse is open; interact with the door again to enter.`;
    case "storehouse-chest":
      if (!has("storehouse-open")) throw new AuthError(403, "Find the storehouse key and follow the delivery slip first.");
      if (has(id)) return "The chest is empty. Read the copied ledger leaf in Keepsakes.";
      mark(id); gear("etched-focus"); supplies("gilded", 1); note("storehouse-record");
      return "Found: Etched Focus (+2 Attack), 1 gilded talisman, and a copied ledger leaf. Its stamp matches your debt notice. Read it in Keepsakes.";
    default:
      if (id.startsWith("track:") && TASKS.includes(id.slice(6))) {
        e.tracked = id.slice(6); return "Journal task tracked.";
      }
      throw new AuthError(400, "Unknown discovery or request.");
  }
}

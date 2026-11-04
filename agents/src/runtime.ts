/**
 * How an agent thinks (plan section 7.8):
 *   1. observe  -> state snapshot
 *   2. options  -> the engine lists only legal actions (agents never invent actions)
 *   3. choose   -> the LLM picks one, in character, with a one-line reason
 *   4. check    -> guardrails: the choice must be one of the options
 *   5. act      -> the engine applies it (later: signed with the agent's limited NEAR key)
 *
 * Numbers are deterministic; words are AI. If the LLM is slow, missing or returns
 * nonsense, the scripted policy decides instead, so the game never stalls.
 */
import {
  legalActions, scriptedPolicy, summarize, renderRule, curseById, WARD_INFO,
  type World, type Participant, type ParticipantAction, type Rng,
} from "@grudgebound/shared";
import { chat, type LlmConfig } from "./nearai";

export interface Decision {
  action: ParticipantAction;
  reason: string;
  source: "llm" | "scripted";
}

const PERSONAS: Record<Participant["personality"], string> = {
  hoarder: "Old Mother Fen, a hunched hoarder who buys up talismans when they are cheap and loves taxing trades she doesn't make.",
  zealot: "Brother Vane, a stern exorcist who hunts the strongest curses and wants PvP banned.",
  schemer: "Silk, a smiling schemer who profits from tolls, taxes and the occasional forged talisman.",
  mercenary: "Rook & Ash, a mercenary pair who raid other players and put bounties on the leader.",
  gambler: "Pell, a gambler who takes wild risks and invests in bankrupt shops.",
  player: "an ordinary player of the Rite.",
};

export function describeAction(world: World, a: ParticipantAction): string {
  switch (a.type) {
    case "idle": return "wait";
    case "hunt": return "hunt a curse in this Ward";
    case "sell": return `sell a ${curseById(a.speciesId).name} talisman to ${world.merchants.find((m) => m.id === a.merchantId)?.name}`;
    case "buy": return `buy ${a.itemId} from ${world.merchants.find((m) => m.id === a.merchantId)?.name}`;
    case "pvp": return `challenge ${world.participants.find((p) => p.id === a.targetId)?.name}`;
    case "move": return `move to ${WARD_INFO[a.ward].name}`;
    case "invest": return `invest Ember to reopen ${world.merchants.find((m) => m.id === a.merchantId)?.name}`;
    case "proposeRule": return `propose the rule "${renderRule(a.templateId, a.params)}"`;
  }
}

export function observe(world: World, me: Participant) {
  const s = summarize(world);
  return {
    you: { name: me.name, ward: me.ward, points: me.points, ember: me.ember, talismans: me.talismans.length },
    time: { day: s.day, hour: s.hour, night: s.night },
    wards: s.wards,
    rules: s.rules.map((r) => r.text),
    leaderboard: s.leaderboard.slice(0, 5).map((p) => `${p.name}: ${p.points}`),
  };
}

export async function decide(world: World, me: Participant, rng: Rng, llm: LlmConfig | null): Promise<Decision> {
  const options = legalActions(world, me);
  const fallback = (): Decision => {
    const action = scriptedPolicy(world, me, options, rng);
    return { action, reason: `(scripted) ${describeAction(world, action)}`, source: "scripted" };
  };
  if (!llm) return fallback();

  const menu = options.map((o, i) => `${i}: ${describeAction(world, o)}`).join("\n");
  try {
    const text = await chat(llm, [
      { role: "system", content: `You are ${PERSONAS[me.personality]} You play a game called the Rite. Stay in character. Reply ONLY with JSON: {"choice": <option number>, "reason": "<one short sentence in character>"}` },
      { role: "user", content: `State:\n${JSON.stringify(observe(world, me))}\n\nYour options:\n${menu}` },
    ]);
    const json = JSON.parse(text.slice(text.indexOf("{"), text.lastIndexOf("}") + 1)) as { choice?: unknown; reason?: unknown };
    const idx = Number(json.choice);
    const action = options[idx];
    // Guardrail: only options the engine offered are accepted.
    if (!Number.isInteger(idx) || !action) {
      if (process.env.NEAR_AI_DEBUG) console.warn(`[llm] ${me.name}: option ${String(json.choice)} not offered`);
      return fallback();
    }
    return { action, reason: String(json.reason ?? "").slice(0, 200), source: "llm" };
  } catch (err) {
    if (process.env.NEAR_AI_DEBUG) console.warn(`[llm] ${me.name}: ${String(err).slice(0, 300)}`);
    return fallback();
  }
}

/**
 * The rival, Dace Marrow (draft): three staged fights kept as exploration flags inside the Satchel store.
 * Every line here is a DRAFT for the owner's review. The server decides where Dace stands and whether a fight is open;
 * the client only places the figure and shows the line.
 */
import { RIVAL_NAME, RIVAL_STAGE, curseById } from "@grudgebound/shared";
import type { Exploration } from "./exploration";

export const RIVAL_FLAGS = ["rival-1-won", "rival-1-lost", "rival-2-won", "rival-3-won"];
/** Progress made before the rival existed: those players already crossed to Tide and keep the ferry. */
const TIDE_HISTORY = ["net-accepted", "reed-bundle", "dock-tin", "door-seen", "tide-cleared"];

export type RivalSpot = "pier" | "ferry" | "quay" | "plaza";
export interface RivalView {
  name: string;
  where: RivalSpot;
  /** The stage that can be fought now, or 0. */
  stage: number;
  ready: boolean;
  level: number;
  speciesId: string;
  line: string;
  results: Record<number, "won" | "lost" | "">;
}

const has = (e: Exploration, f: string) => e.flags.includes(f);
export function tideOpen(e: Exploration): boolean {
  return has(e, "rival-2-won") || TIDE_HISTORY.some((f) => has(e, f));
}
function stageDone(e: Exploration, stage: number): boolean {
  return stage === 1 ? has(e, "rival-1-won") || has(e, "rival-1-lost") : has(e, `rival-${stage}-won`);
}

/** Why the given stage cannot be fought now, or null. */
export function rivalRefusal(e: Exploration, stage: unknown, arrived: boolean): string | null {
  if (!Number.isSafeInteger(stage) || !RIVAL_STAGE[stage as number]) return "Unknown rival stage.";
  const s = stage as number;
  if (!arrived) return "Finish your arrival with Kessa first.";
  if (stageDone(e, s)) return `${RIVAL_NAME} has already fought you there.`;
  if (s === 2 && !stageDone(e, 1)) return `${RIVAL_NAME} is waiting for you on the harbour pier first.`;
  if (s === 3 && !has(e, "rival-2-won")) return `Beat ${RIVAL_NAME} at the ferry first.`;
  if (s === 3 && !has(e, "storehouse-chest")) return `${RIVAL_NAME}: "Open that storehouse first. Then we talk."`;
  return null;
}

export function rivalView(e: Exploration, starter: string, playerName: string, arrived: boolean): RivalView {
  const results: RivalView["results"] = {
    1: has(e, "rival-1-won") ? "won" : has(e, "rival-1-lost") ? "lost" : "",
    2: has(e, "rival-2-won") ? "won" : "",
    3: has(e, "rival-3-won") ? "won" : "",
  };
  const stage = !stageDone(e, 1) ? 1 : !stageDone(e, 2) ? 2 : !stageDone(e, 3) ? 3 : 0;
  const ready = stage > 0 && rivalRefusal(e, stage, arrived) === null;
  const where: RivalSpot = stage === 1 ? "pier" : stage === 2 ? "ferry" : stage === 3 ? "quay" : "plaza";
  const partner = curseById(starter).name;
  const you = playerName || "newcomer";
  const line = stage === 1
    ? `Kessa gave you a ${partner} too? Then we find out whose takes to it. One round, ${you}, right here on the pier.`
    : stage === 2
      ? results[1] === "lost"
        ? `You fell on the pier, and I haven't forgotten. The ferryman takes nobody green. Show me that was bad luck.`
        : `The ferryman takes nobody green across, and I've been training. Show me you have too, ${you}.`
      : stage === 3
        ? ready
          ? `You opened it. That stamp on the ledger leaf matches my notice as well as yours. One more round, and the winner asks the Rite Hall who kept the original.`
          : `You crossed. Fine. Folk on this quay say a sealed storehouse keeps copies of everyone's debts. Get in there before I do.`
        : `Three rounds, ${you}. The Rite Hall still won't say who kept the original. You watch the ledger; I'll watch the Calamity board.`;
  return { name: RIVAL_NAME, where, stage, ready, level: stage ? RIVAL_STAGE[stage]!.level : RIVAL_STAGE[3]!.level, speciesId: starter, line, results };
}

/** The rival's words when a fight ends: by stage, result and how close it was. */
export function rivalEndLine(stage: number, outcome: "won" | "lost", hpLeftFraction: number, playerName: string): string {
  const you = playerName || "newcomer";
  if (outcome === "lost") return [
    `Not today, ${you}. Go and see the market; Kessa will want you fed before our next round.`,
    `Too green for the crossing. Rest your partner and come back.`,
    `Then the storehouse keeps its secret a little longer. Rest. I'll be here.`,
  ][stage - 1]!;
  if (hpLeftFraction <= 0.25) return [
    `A hair's width. Next time it goes my way.`,
    `You barely held. Take the ferry before I change my mind.`,
    `That close? Fine. You ask the Rite Hall. I'll be listening.`,
  ][stage - 1]!;
  return [
    `...Beginner's luck. Our partners will grow. Mine faster.`,
    `You did train. Go on, the ferry is yours.`,
    `That's twice I owe you. The Rite Hall, then. You ask; I'll listen.`,
  ][stage - 1]!;
}

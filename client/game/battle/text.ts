/** Battle log wording. Pure functions, so the log is part of the replayable result. */
import type { BattleEvent, BattleState, Outcome, PlayerAction } from "@grudgebound/shared";

export function pct(p: number): string {
  return `${Math.round(p * 100)}%`;
}

/** "Wild Hushmoth", but plain "The Shuttered" (names that already carry an article stay as they are). */
export function wildName(name: string, capital = true): string {
  if (name.startsWith("The ")) return name;
  return `${capital ? "Wild" : "wild"} ${name}`;
}

export function introLine(state: BattleState): string {
  return `A ${state.wild.omen} rises: ${wildName(state.wild.name, false)}! Your ${state.player.name} stands ready.`;
}

export function actionLabel(a: PlayerAction): string {
  switch (a.kind) {
    case "attack": return "attack";
    case "ward": return "ward";
    case "seal": return a.gilded ? "seal (gilded)" : "seal";
    case "item": return "ink salve";
    case "flee": return "slip away";
  }
}

/** One log line per event. `state` is the state after the turn (names do not change). */
export function describeEvent(e: BattleEvent, state: BattleState): string {
  const name = (side: "player" | "wild") => (side === "player" ? `Your ${state.player.name}` : wildName(state.wild.name));
  switch (e.kind) {
    case "attack": {
      const target = e.by === "player" ? state.wild.name : state.player.name;
      let line = `${name(e.by)} strikes ${target} for ${e.damage}.`;
      if (e.crit) line += " A cruel blow!";
      if (e.multiplier > 1) line += " Its grudge cuts deep.";
      else if (e.multiplier < 1) line += " The grudge barely lands.";
      if (e.warded) line += " (warded)";
      return line;
    }
    case "ward":
      return e.by === "player" ? `Your ${state.player.name} raises a paper ward.` : `${wildName(state.wild.name)} curls up behind a ward.`;
    case "heal":
      return `Ink salve mends ${state.player.name} for ${e.amount}.`;
    case "seal":
      return e.success
        ? `The ${e.gilded ? "gilded " : ""}talisman takes hold (${pct(e.chance)}). The wax sets!`
        : `The ${e.gilded ? "gilded " : ""}talisman tears (${pct(e.chance)}). It breaks free!`;
    case "flee":
      return e.success ? `You slip away into the crowd (${pct(e.chance)}).` : `No way out! (${pct(e.chance)})`;
    case "faint":
      return e.who === "wild" ? `${state.wild.name} unravels into ash and paper.` : `${state.player.name} crumples. You retreat.`;
    case "end":
      return outcomeLine(e.outcome, state);
  }
}

export function outcomeLine(o: Outcome, state: BattleState): string {
  switch (o) {
    case "won": return `Exorcised. The ${state.wild.omen} fades from the Ward.`;
    case "bound": return `${state.wild.name} is bound into your talisman.`;
    case "fled": return "The grudge lingers behind you.";
    case "lost": return "The curse keeps its corner of the Ward tonight.";
  }
}

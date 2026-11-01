/**
 * BattleScene: the turn-based fight between your bound curse and a wild one.
 *
 * HOW WARDSCENE SHOULD CALL IT (to wire up later; WardScene is not changed yet):
 *
 *   // in WardScene.encounter(), instead of openDialog(curse):
 *   this.scene.pause();                                   // freeze walking + Ward input
 *   this.scene.launch("battle", {
 *     wild: curse.id,                                     // CurseSpecies id, e.g. "gutterjaw"
 *     omen,                                               // "whisper" | "wail" | "calamity" (defaults to the species' omen)
 *     playerCurse: "cinder-grin",                         // optional: the player's bound curse id
 *     seed: this.rng.int(0, 2 ** 31),                     // optional: makes the battle replayable
 *   } satisfies BattleData);
 *   this.game.events.once("battle-end", (r: BattleResult) => {
 *     // r.outcome: "won" | "fled" | "bound" | "lost"; r.seed + r.actions replay it; r.log is the text log
 *     this.scene.resume();
 *   });
 *
 * The battle stops itself after its closing paper-tear transition, then emits "battle-end" on
 * both `this.game.events` and its own `this.events`. BattleScene must be in the game's scene
 * list (see game/start.ts). It draws all of its own textures, so it needs no BootScene assets.
 *
 * Testing: open the page with `?scene=battle` (optionally `&wild=riot-bell&seed=7&player=hushmoth
 * &reducedMotion=1`). In that mode the scene starts on its own and offers a rematch at the end.
 *
 * Controls: arrow keys move, Enter/Space confirm, 1-4 pick directly, Esc/Backspace goes back
 * from the Item menu. Mouse hover + click also work. Enter/Space/click also skip the typewriter.
 */
import * as Phaser from "phaser";
import { CURSES, EMOTION_INFO, OMENS, createRng, curseById, type Omen, type Rng } from "@grudgebound/shared";
import {
  canUse, createBattle, fleeChance, resolveTurn, sealChance,
  type BattleEvent, type BattleState, type Outcome, type PlayerAction, type Side,
} from "../battle/resolve";
import { actionLabel, describeEvent, introLine, pct } from "../battle/text";
import { backdropTexture, BACKDROP_SCALE, PLAYER_SPOT, WILD_SPOT } from "../battle/backdrop";
import { curseTexture } from "../battle/curseSprites";
import { Frame, ensureGrain, iconTexture, kessaTexture } from "../battle/art";
import { ActionMenu, BattleLog, HealthCard, textStyle, type MenuOption } from "../battle/ui";
import { PaperTear, burst, damageNumber, hitFlash, sealRite, shake, slash, tween, wait } from "../battle/fx";
import { C, hexToInt } from "../battle/pixels";

export interface BattleData {
  /** Wild curse species id (see CURSES in @grudgebound/shared). */
  wild: string;
  omen?: Omen;
  /** The player's bound curse species id. Defaults to a starter (see DEFAULT_PLAYER_CURSE). */
  playerCurse?: string;
  /** Seed for the battle's Rng. Defaults to a time-based seed; it is returned in the result. */
  seed?: number;
  /** Force reduced motion on/off. Defaults to the OS "prefers-reduced-motion" setting. */
  reducedMotion?: boolean;
  /** Standalone test mode (?scene=battle): offer a rematch instead of stopping. */
  standalone?: boolean;
}

export interface BattleResult {
  outcome: Outcome;
  seed: number;
  log: string[];
  /** Player actions in order: replayBattle(setup, seed, actions) reproduces the fight. */
  actions: PlayerAction[];
  wild: string;
  playerCurse: string;
}

export const BATTLE_END = "battle-end";

const W = 960;
const H = 540;
const PANEL_Y = 376;
const PLAYER_SCALE = 4;
const WILD_SCALE = 3;

type Mode = "intro" | "menu" | "items" | "busy" | "done";

interface Fighter {
  holder: Phaser.GameObjects.Container;
  sprite: Phaser.GameObjects.Image;
  card: HealthCard;
  ward?: Phaser.GameObjects.Image;
}

/** Reads ?scene=battle&wild=..&omen=..&player=..&seed=..&reducedMotion=1 for standalone testing. */
export function battleDataFromUrl(search: string): BattleData | null {
  const q = new URLSearchParams(search);
  if (q.get("scene") !== "battle") return null;
  const ids = CURSES.map((c) => c.id);
  const seedParam = q.get("seed");
  const seed = seedParam !== null && Number.isFinite(Number(seedParam)) ? Number(seedParam) >>> 0 : Date.now() >>> 0;
  const wildParam = q.get("wild");
  const wild = wildParam && ids.includes(wildParam) ? wildParam : createRng(seed ^ 0x9e3779b9).pick(ids.filter((id) => curseById(id).omen !== "calamity"));
  const playerParam = q.get("player");
  const omenParam = q.get("omen") as Omen | null;
  const rm = q.get("reducedMotion");
  return {
    wild, seed, standalone: true,
    playerCurse: playerParam && ids.includes(playerParam) ? playerParam : undefined,
    omen: omenParam && (OMENS as readonly string[]).includes(omenParam) ? omenParam : undefined,
    reducedMotion: rm === null ? undefined : rm === "1" || rm === "true",
  };
}

export class BattleScene extends Phaser.Scene {
  private data0!: BattleData;
  private seed = 0;
  private rng!: Rng;
  private state!: BattleState;
  private actions: PlayerAction[] = [];
  private reduced = false;
  private mode: Mode = "intro";
  private player!: Fighter;
  private wild!: Fighter;
  private log!: BattleLog;
  private menu!: ActionMenu;
  private tear!: PaperTear;
  private hint!: Phaser.GameObjects.Text;
  private endConfirm?: () => void;
  private lastMain = 0;

  constructor() {
    super("battle");
  }

  init(data: Partial<BattleData> | undefined) {
    const fromUrl = typeof window !== "undefined" ? battleDataFromUrl(window.location.search) : null;
    this.data0 = data && data.wild ? (data as BattleData) : (fromUrl ?? { wild: "weepwick", standalone: true });
    this.seed = (this.data0.seed ?? Date.now()) >>> 0;
    this.rng = createRng(this.seed);
    this.state = createBattle({ wild: this.data0.wild, omen: this.data0.omen, playerCurse: this.data0.playerCurse });
    this.actions = [];
    this.mode = "intro";
    this.endConfirm = undefined;
    this.lastMain = 0;
    const prefers = typeof window !== "undefined" && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches;
    this.reduced = this.data0.reducedMotion ?? !!prefers;
  }

  create() {
    this.cameras.main.setBackgroundColor(C.ink);
    this.add.image(0, 0, backdropTexture(this)).setOrigin(0, 0).setScale(BACKDROP_SCALE);
    this.buildKessa();
    this.wild = this.buildFighter("wild");
    this.player = this.buildFighter("player");
    this.buildPanel();
    this.bindInput();
    this.tear = new PaperTear(this, this.reduced);
    void this.intro();
  }

  // ---------------------------------------------------------------- building

  private buildKessa() {
    const k = this.add.image(472, 64, kessaTexture(this)).setScale(2).setDepth(5);
    if (!this.reduced) this.tweens.add({ targets: k, y: 72, angle: { from: -4, to: 4 }, duration: 1600, yoyo: true, repeat: -1, ease: "Sine.easeInOut" });
  }

  private buildFighter(side: Side): Fighter {
    const c = side === "player" ? this.state.player : this.state.wild;
    const spot = side === "player" ? PLAYER_SPOT : WILD_SPOT;
    const scale = side === "player" ? PLAYER_SCALE : WILD_SCALE;
    const { key, size } = curseTexture(this, c.speciesId);
    const holder = this.add.container(spot.x * BACKDROP_SCALE, spot.y * BACKDROP_SCALE).setDepth(side === "player" ? 20 : 10);
    const shadow = this.add.ellipse(0, 0, size * scale * 0.7, 18, hexToInt(C.ink), 0.22);
    const sprite = this.add.image(0, 4, key).setOrigin(0.5, 1).setScale(scale);
    holder.add([shadow, sprite]);
    if (!this.reduced) {
      this.tweens.add({ targets: sprite, y: -6, duration: side === "player" ? 1100 : 900, yoyo: true, repeat: -1, ease: "Sine.easeInOut", delay: side === "player" ? 300 : 0 });
      this.tweens.add({ targets: shadow, scaleX: 0.88, duration: side === "player" ? 1100 : 900, yoyo: true, repeat: -1, ease: "Sine.easeInOut", delay: side === "player" ? 300 : 0 });
    }
    const card = side === "player"
      ? new HealthCard(this, 596, 272, 340, 92, c, true)
      : new HealthCard(this, 24, 20, 340, 90, c, false);
    card.setDepth(30);
    return { holder, sprite, card };
  }

  private buildPanel() {
    new Frame(this, 8, PANEL_Y, W - 16, H - PANEL_Y - 8, "panel").setDepth(30);
    this.add.tileSprite(16, PANEL_Y + 8, W - 32, H - PANEL_Y - 24, ensureGrain(this)).setOrigin(0, 0).setDepth(30).setAlpha(0.6);
    this.log = new BattleLog(this, 22, PANEL_Y + 14, 424, H - PANEL_Y - 36, this.reduced ? 10 : 22);
    this.log.setDepth(31);
    this.menu = new ActionMenu(this, 458, PANEL_Y + 14, 236, 60, 8, this.reduced);
    this.menu.onConfirm = (i) => this.onMenu(i);
    this.hint = this.add.text(W - 16, PANEL_Y - 4, "", { ...textStyle(12, C.paper), stroke: C.ink, strokeThickness: 4 }).setOrigin(1, 1).setDepth(31);
  }

  private bindInput() {
    const kb = this.input.keyboard;
    if (!kb) return;
    kb.on("keydown", (e: KeyboardEvent) => {
      const code = e.code;
      if (code === "Enter" || code === "Space" || code === "NumpadEnter") {
        e.preventDefault();
        if (this.log.typing) return this.log.skip();
        if (this.mode === "menu" || this.mode === "items") return this.menu.confirm();
        if (this.mode === "done") return this.endConfirm?.();
        return;
      }
      if (this.mode !== "menu" && this.mode !== "items") return;
      const digit = ["Digit1", "Digit2", "Digit3", "Digit4"].indexOf(code);
      const numpad = ["Numpad1", "Numpad2", "Numpad3", "Numpad4"].indexOf(code);
      const n = Math.max(digit, numpad);
      if (n >= 0) {
        this.menu.select(n);
        this.menu.confirm();
      } else if (code === "ArrowLeft" || code === "KeyA") this.menu.move(-1, 0);
      else if (code === "ArrowRight" || code === "KeyD") this.menu.move(1, 0);
      else if (code === "ArrowUp" || code === "KeyW") this.menu.move(0, -1);
      else if (code === "ArrowDown" || code === "KeyS") this.menu.move(0, 1);
      else if ((code === "Escape" || code === "Backspace") && this.mode === "items") this.showMain();
      if (code.startsWith("Arrow")) e.preventDefault();
    });
    this.input.on("pointerdown", (_p: Phaser.Input.Pointer, over: Phaser.GameObjects.GameObject[]) => {
      if (over.length) return; // buttons handle themselves
      if (this.log.typing) this.log.skip();
      else if (this.mode === "done") this.endConfirm?.();
    });
  }

  // ---------------------------------------------------------------- flow

  private async intro() {
    this.menu.setActive(false);
    this.showMain(false);
    const wildX = this.wild.holder.x, playerX = this.player.holder.x;
    if (!this.reduced) {
      this.wild.holder.x = W + 200;
      this.player.holder.x = -200;
    }
    await this.tear.open();
    if (!this.reduced) {
      await Promise.all([
        tween(this, { targets: this.wild.holder, x: wildX, duration: 520, ease: "Back.easeOut" }),
        tween(this, { targets: this.player.holder, x: playerX, duration: 520, ease: "Back.easeOut", delay: 120 }),
      ]);
    }
    await this.say(introLine(this.state));
    this.startPlayerTurn();
  }

  private startPlayerTurn() {
    this.mode = "menu";
    this.menu.index = this.lastMain;
    this.showMain();
    this.hint.setText("Arrows + Enter  ·  1-4  ·  click");
  }

  private mainOptions(): MenuOption[] {
    const s = this.state;
    const seal = sealChance(s.wild);
    const itemsLeft = s.items.salve + s.items.gilded;
    return [
      { label: "Attack", icon: "attack", sub: `${EMOTION_INFO[s.player.emotion].label} strike`, enabled: true },
      { label: "Ward", icon: "ward", sub: "halve the next hit", enabled: true },
      { label: "Seal", icon: "seal", sub: seal > 0 ? `bind chance ${pct(seal)}` : "weaken it first", enabled: true },
      { label: "Item", icon: "item", sub: itemsLeft ? `${itemsLeft} in satchel` : "slip away", enabled: true },
    ];
  }

  private itemOptions(): MenuOption[] {
    const s = this.state;
    return [
      { label: "Ink Salve", icon: "salve", sub: `x${s.items.salve} · heal 40%`, enabled: s.items.salve > 0 && s.player.hp < s.player.maxHp },
      { label: "Gilded Seal", icon: "gilded", sub: `x${s.items.gilded} · ${pct(sealChance(s.wild, true))}`, enabled: s.items.gilded > 0 && sealChance(s.wild, true) > 0 },
      { label: "Slip Away", icon: "flee", sub: `flee ${pct(fleeChance(s))}`, enabled: true },
      { label: "Back", icon: "back", sub: "Esc", enabled: true },
    ];
  }

  private showMain(active = true) {
    if (this.mode === "items") this.mode = "menu";
    this.menu.setOptions(this.mainOptions());
    this.menu.setActive(active && this.mode === "menu");
  }

  private onMenu(i: number) {
    if (this.mode === "menu") {
      this.lastMain = i;
      if (i === 0) return void this.takeTurn({ kind: "attack" });
      if (i === 1) return void this.takeTurn({ kind: "ward" });
      if (i === 2) return void this.takeTurn({ kind: "seal" });
      this.mode = "items";
      this.menu.index = 0;
      this.menu.setOptions(this.itemOptions());
      this.menu.setActive(true);
      return;
    }
    if (this.mode === "items") {
      if (i === 0) return void this.takeTurn({ kind: "item", item: "salve" });
      if (i === 1) return void this.takeTurn({ kind: "seal", gilded: true });
      if (i === 2) return void this.takeTurn({ kind: "flee" });
      this.menu.index = 3;
      this.showMain();
    }
  }

  private async takeTurn(action: PlayerAction) {
    if (!canUse(this.state, action)) return;
    this.mode = "busy";
    this.menu.setActive(false);
    this.hint.setText("");
    const { state, events } = resolveTurn(this.state, action, this.rng);
    this.actions.push(action);
    const before = this.state;
    this.state = state;
    for (const e of events) await this.play(e, before);
    this.clearWards();
    if (state.outcome) return this.finish(state.outcome);
    this.startPlayerTurn();
  }

  // ---------------------------------------------------------------- animating events

  private fighter(side: Side): Fighter {
    return side === "player" ? this.player : this.wild;
  }

  private center(side: Side): { x: number; y: number } {
    const f = this.fighter(side);
    return { x: f.holder.x, y: f.holder.y - f.sprite.displayHeight / 2 };
  }

  private async play(e: BattleEvent, before: BattleState) {
    switch (e.kind) {
      case "attack": {
        const atk = this.fighter(e.by), defSide: Side = e.by === "player" ? "wild" : "player";
        const def = this.fighter(defSide);
        const dx = e.by === "player" ? 70 : -70, dy = e.by === "player" ? -40 : 30;
        if (!this.reduced) await tween(this, { targets: atk.holder, x: atk.holder.x + dx, y: atk.holder.y + dy, duration: 140, yoyo: true, ease: "Quad.easeOut", hold: 40 });
        const c = this.center(defSide);
        const attacker = e.by === "player" ? this.state.player : this.state.wild;
        slash(this, c.x, c.y, EMOTION_INFO[attacker.emotion].color, e.by === "wild");
        burst(this, c.x, c.y, [EMOTION_INFO[attacker.emotion].color, C.ink, C.paper], 12, 70, this.reduced);
        shake(this, this.reduced, e.crit || e.multiplier > 1);
        damageNumber(this, c.x + (e.by === "player" ? -30 : 30), c.y - 30, `-${e.damage}`, e.multiplier > 1 ? C.crimson : C.ink, this.reduced);
        void hitFlash(this, def.sprite, this.reduced);
        if (!this.reduced) void tween(this, { targets: def.holder, x: def.holder.x + (e.by === "player" ? 10 : -10), duration: 60, yoyo: true, repeat: 1 });
        def.card.setHp(e.hpAfter);
        await this.say(describeEvent(e, this.state));
        break;
      }
      case "ward": {
        const f = this.fighter(e.by);
        const c = this.center(e.by);
        const color = EMOTION_INFO[(e.by === "player" ? this.state.player : this.state.wild).emotion].color;
        const ward = this.add.image(c.x + (e.by === "player" ? 70 : -70), c.y, iconTexture(this, "ward")).setScale(this.reduced ? 5 : 1).setDepth(25).setAlpha(0.9);
        f.ward = ward;
        f.card.setStatus("warded");
        const ring = this.add.ellipse(c.x, c.y, 40, 40).setStrokeStyle(4, hexToInt(color)).setDepth(24);
        if (!this.reduced) {
          void tween(this, { targets: ward, scale: 5, duration: 260, ease: "Back.easeOut" });
          void tween(this, { targets: ring, scale: 5, alpha: 0, duration: 520, onComplete: () => ring.destroy() });
        } else ring.destroy();
        await this.say(describeEvent(e, this.state));
        break;
      }
      case "heal": {
        const c = this.center("player");
        burst(this, c.x, c.y, [C.gold, C.goldLight, C.white], 14, 60, this.reduced);
        damageNumber(this, c.x, c.y - 40, `+${e.amount}`, C.goldDark, this.reduced);
        this.player.card.setHp(e.hpAfter);
        await this.say(describeEvent(e, this.state));
        break;
      }
      case "seal": {
        const from = this.center("player"), to = this.center("wild");
        await sealRite(this, {
          fromX: from.x + 40, fromY: from.y - 20, x: to.x, y: to.y,
          sprite: this.wild.sprite, gilded: e.gilded, success: e.success, reduced: this.reduced,
        });
        await this.say(describeEvent(e, this.state));
        break;
      }
      case "flee": {
        if (e.success) {
          if (!this.reduced) await tween(this, { targets: this.player.holder, x: -220, duration: 420, ease: "Quad.easeIn" });
          else this.player.holder.setAlpha(0.3);
        } else if (!this.reduced) {
          await tween(this, { targets: this.player.holder, x: this.player.holder.x - 30, duration: 120, yoyo: true });
        }
        await this.say(describeEvent(e, this.state));
        break;
      }
      case "faint": {
        const f = this.fighter(e.who);
        const c = this.center(e.who);
        this.tweens.killTweensOf(f.sprite);
        burst(this, c.x, c.y, [C.paper, C.paperShade, C.ink, EMOTION_INFO[(e.who === "player" ? before.player : before.wild).emotion].color], 24, 110, this.reduced);
        await tween(this, { targets: f.sprite, scaleY: 0, alpha: 0, duration: this.reduced ? 250 : 600, ease: "Quad.easeIn" });
        await this.say(describeEvent(e, this.state));
        break;
      }
      case "end":
        await this.say(describeEvent(e, this.state));
        break;
    }
  }

  private clearWards() {
    for (const f of [this.player, this.wild]) {
      if (f.ward) {
        const w = f.ward;
        f.ward = undefined;
        void tween(this, { targets: w, alpha: 0, duration: 200, onComplete: () => w.destroy() });
      }
      f.card.setStatus("");
    }
  }

  private async say(line: string) {
    await this.log.say(line);
    await wait(this, this.reduced ? 350 : 500);
  }

  // ---------------------------------------------------------------- ending

  private async finish(outcome: Outcome) {
    this.mode = "done";
    if (outcome === "bound") this.wild.card.setStatus("bound", C.crimson);
    else if (outcome === "won") this.wild.card.setStatus("exorcised", C.crimson);
    this.log.showCaret(true);
    this.hint.setText(this.data0.standalone ? "Enter: fight again" : "Enter: continue");
    await new Promise<void>((resolve) => { this.endConfirm = resolve; });
    this.endConfirm = undefined;
    this.log.showCaret(false);
    this.hint.setText("");
    await this.tear.close();
    const result: BattleResult = {
      outcome, seed: this.seed, log: [...this.log.all], actions: [...this.actions],
      wild: this.state.wild.speciesId, playerCurse: this.state.player.speciesId,
    };
    // keep the action list readable in the dev console
    if (process.env.NODE_ENV !== "production") console.info("[battle]", outcome, this.seed, this.actions.map(actionLabel).join(", "));
    this.events.emit(BATTLE_END, result);
    this.game.events.emit(BATTLE_END, result);
    if (this.data0.standalone) {
      const next = battleDataFromUrl(window.location.search) ?? { wild: this.data0.wild, standalone: true };
      const seed = (this.seed + 1) >>> 0;
      const wild = new URLSearchParams(window.location.search).get("wild")
        ? next.wild
        : createRng(seed).pick(CURSES.filter((c) => c.omen !== "calamity")).id;
      this.scene.restart({ ...next, wild, seed });
    } else {
      this.scene.stop();
    }
  }
}

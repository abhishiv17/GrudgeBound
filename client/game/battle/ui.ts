/** Battle screen widgets: health cards, the typewriter log and the 2x2 action menu. */
import * as Phaser from "phaser";
import { EMOTION_INFO, type Emotion, type Omen } from "@grudgebound/shared";
import { C, hexToInt } from "./pixels";
import { Frame, emotionIcon, ensureGrain, iconTexture, omenPip, type IconId } from "./art";

export const FONT_FAMILY = '"Courier New", Courier, monospace';

export function textStyle(size: number, color: string = C.ink, bold = true): Phaser.Types.GameObjects.Text.TextStyle {
  return { fontFamily: FONT_FAMILY, fontSize: `${size}px`, color, fontStyle: bold ? "bold" : "normal" };
}

const OMEN_TIER: Record<Omen, number> = { whisper: 1, wail: 2, calamity: 3 };
const cap = (s: string) => s[0]!.toUpperCase() + s.slice(1);

// ------------------------------------------------------------------ health card

export interface CardInfo {
  name: string;
  emotion: Emotion;
  omen: Omen;
  maxHp: number;
  hp: number;
}

/** Paper slip with name, emotion glyph, omen tier and a pixel-bordered HP bar. */
export class HealthCard extends Phaser.GameObjects.Container {
  private bar: Phaser.GameObjects.Graphics;
  private hpText?: Phaser.GameObjects.Text;
  private status: Phaser.GameObjects.Text;
  private shown: number;
  private ghost: number;
  private readonly barX = 22;
  private readonly barY: number;
  private readonly barW: number;

  constructor(scene: Phaser.Scene, x: number, y: number, w: number, h: number, private info: CardInfo, showNumbers: boolean) {
    super(scene, x, y);
    this.shown = info.hp;
    this.ghost = info.hp;
    const frame = new Frame(scene, 0, 0, w, h, "card");
    const grain = scene.add.tileSprite(6, 6, w - 12, h - 12, ensureGrain(scene)).setOrigin(0, 0).setAlpha(0.7);
    const accent = scene.add.graphics();
    const emo = hexToInt(EMOTION_INFO[info.emotion].color);
    accent.fillStyle(emo, 1).fillRect(6, 6, 6, h - 12);
    accent.fillStyle(hexToInt(C.ink), 0.25).fillRect(12, 6, 1, h - 12);
    const icon = scene.add.image(28, 22, emotionIcon(scene, info.emotion)).setScale(2);
    const name = scene.add.text(44, 11, info.name, textStyle(19));
    const tier = OMEN_TIER[info.omen];
    const pips = [0, 1, 2].map((i) => scene.add.image(w - 90 + i * 14, 21, omenPip(scene, i < tier)).setScale(2));
    const omenLabel = scene.add.text(w - 16, 31, cap(info.omen), textStyle(12, omenColor(info.omen))).setOrigin(1, 0);
    const emoLabel = scene.add.text(44, 32, EMOTION_INFO[info.emotion].label, textStyle(12, EMOTION_INFO[info.emotion].color));
    this.barY = 50;
    this.barW = w - this.barX - 16;
    this.bar = scene.add.graphics();
    this.status = scene.add.text(w - 16, this.barY + 18, "", textStyle(13, C.sorrow)).setOrigin(1, 0);
    this.add([frame, grain, accent, icon, name, emoLabel, ...pips, omenLabel, this.bar, this.status]);
    if (showNumbers) {
      this.hpText = scene.add.text(this.barX, h - 24, "", textStyle(14));
      this.add(this.hpText);
    }
    this.drawBar();
    scene.add.existing(this);
  }

  /** Animates the bar to the new HP; the gold "ghost" shows the chunk just lost, then drains. */
  setHp(hp: number, instant = false): void {
    this.scene.tweens.killTweensOf(this);
    if (instant) {
      this.shown = this.ghost = hp;
      this.drawBar();
      return;
    }
    const losing = hp < this.shown;
    if (!losing) this.ghost = hp;
    this.scene.tweens.add({
      targets: this, shown: hp, duration: 380, ease: "Cubic.easeOut",
      onUpdate: () => this.drawBar(),
      onComplete: () => {
        this.scene.tweens.add({ targets: this, ghost: hp, delay: 260, duration: 420, onUpdate: () => this.drawBar() });
      },
    });
  }

  setStatus(text: string, color: string = C.sorrow): void {
    this.status.setText(text).setColor(color);
  }

  private drawBar() {
    const g = this.bar, x = this.barX, y = this.barY, w = this.barW, h = 14;
    const ink = hexToInt(C.ink);
    g.clear();
    // stepped (notched) pixel border
    g.fillStyle(ink, 1).fillRect(x + 2, y, w - 4, h).fillRect(x, y + 2, w, h - 4);
    g.fillStyle(hexToInt("#3A3350"), 1).fillRect(x + 2, y + 2, w - 4, h - 4);
    const inner = w - 4;
    const frac = Math.max(0, this.shown / this.info.maxHp);
    const ghostFrac = Math.max(frac, this.ghost / this.info.maxHp);
    if (ghostFrac > frac) g.fillStyle(hexToInt(C.goldLight), 1).fillRect(x + 2, y + 2, Math.round(inner * ghostFrac), h - 4);
    const [base, light, dark] = frac > 0.5 ? [C.gold, C.goldLight, C.goldDark] : frac > 0.25 ? [C.rage, C.rageLight, C.rageDark] : [C.crimson, C.crimsonLight, C.crimsonDark];
    const fw = Math.round(inner * frac);
    if (fw > 0) {
      g.fillStyle(hexToInt(base), 1).fillRect(x + 2, y + 2, fw, h - 4);
      g.fillStyle(hexToInt(light), 1).fillRect(x + 2, y + 2, fw, 2);
      g.fillStyle(hexToInt(dark), 1).fillRect(x + 2, y + h - 4, fw, 2);
    }
    g.fillStyle(ink, 0.35);
    for (let i = 1; i < 10; i++) g.fillRect(x + 2 + Math.round((inner * i) / 10), y + 2, 1, 3);
    this.hpText?.setText(`${Math.round(this.shown)} / ${this.info.maxHp}`);
  }
}

function omenColor(o: Omen): string {
  return o === "calamity" ? C.crimson : o === "wail" ? C.goldDark : C.inkSoft;
}

// ------------------------------------------------------------------ battle log

/** Parchment box; the newest line types out, the two before it stay above, faded. */
export class BattleLog extends Phaser.GameObjects.Container {
  private history: Phaser.GameObjects.Text;
  private current: Phaser.GameObjects.Text;
  private caret: Phaser.GameObjects.Graphics;
  private lines: string[] = [];
  private target = "";
  private shownChars = 0;
  private timer?: Phaser.Time.TimerEvent;
  private resolveTyping?: () => void;
  readonly all: string[] = [];

  private boxH: number;

  constructor(scene: Phaser.Scene, x: number, y: number, w: number, h: number, private charMs = 22) {
    super(scene, x, y);
    this.boxH = h;
    const frame = new Frame(scene, 0, 0, w, h, "log");
    const grain = scene.add.tileSprite(8, 8, w - 16, h - 16, ensureGrain(scene)).setOrigin(0, 0).setAlpha(0.5);
    const wrap = { width: w - 40, useAdvancedWrap: true };
    this.history = scene.add.text(18, 14, "", { ...textStyle(14, "#7A6446"), wordWrap: wrap, lineSpacing: 2 });
    this.current = scene.add.text(18, 14, "", { ...textStyle(16, C.ink), wordWrap: wrap, lineSpacing: 3 });
    this.caret = scene.add.graphics().setVisible(false);
    this.caret.fillStyle(hexToInt(C.crimson), 1).fillTriangle(0, 0, 10, 0, 5, 6);
    this.caret.setPosition(w - 26, h - 20);
    scene.tweens.add({ targets: this.caret, y: h - 17, duration: 420, yoyo: true, repeat: -1 });
    this.add([frame, grain, this.history, this.current, this.caret]);
    scene.add.existing(this);
  }

  get typing(): boolean {
    return this.shownChars < this.target.length;
  }

  /** Types a line; resolves once it is fully shown. */
  say(line: string): Promise<void> {
    this.all.push(line);
    if (this.target) this.lines.push(this.target);
    this.lines = this.lines.slice(-2);
    this.history.setText(this.lines.join("\n"));
    this.current.setY(this.lines.length ? 14 + this.history.height + 6 : 14);
    this.target = line;
    this.shownChars = 0;
    this.current.setText("");
    this.caret.setVisible(false);
    this.timer?.remove();
    // keep the box tidy: if history + current would overflow, drop the oldest line
    while (this.lines.length && this.current.y + this.measure(line) > this.boxH - 12) {
      this.lines.shift();
      this.history.setText(this.lines.join("\n"));
      this.current.setY(this.lines.length ? 14 + this.history.height + 6 : 14);
    }
    return new Promise((resolve) => {
      this.resolveTyping = resolve;
      if (this.charMs <= 0) return this.finish();
      this.timer = this.scene.time.addEvent({
        delay: this.charMs, loop: true,
        callback: () => {
          this.shownChars += 1;
          this.current.setText(this.target.slice(0, this.shownChars));
          if (this.shownChars >= this.target.length) this.finish();
        },
      });
    });
  }

  /** Reveals the current line at once. */
  skip(): void {
    if (this.typing) this.finish();
  }

  showCaret(on: boolean): void {
    this.caret.setVisible(on);
  }

  private finish() {
    this.timer?.remove();
    this.timer = undefined;
    this.shownChars = this.target.length;
    this.current.setText(this.target);
    const r = this.resolveTyping;
    this.resolveTyping = undefined;
    r?.();
  }

  private measure(line: string): number {
    const probe = this.current.text;
    this.current.setText(line);
    const height = this.current.height;
    this.current.setText(probe);
    return height;
  }
}

// ------------------------------------------------------------------ action menu

export interface MenuOption {
  label: string;
  icon: IconId;
  sub?: string;
  enabled: boolean;
}

interface Button {
  frame: Frame;
  icon: Phaser.GameObjects.Image;
  label: Phaser.GameObjects.Text;
  sub: Phaser.GameObjects.Text;
  key: Phaser.GameObjects.Text;
  zone: Phaser.GameObjects.Zone;
  box: Phaser.GameObjects.Container;
  enabled: boolean;
}

/** Four paper buttons in a 2x2 grid. Arrow keys / 1-4 / mouse; Enter, Space or click confirms. */
export class ActionMenu {
  private buttons: Button[] = [];
  index = 0;
  active = false;
  onConfirm: (index: number) => void = () => {};

  constructor(private scene: Phaser.Scene, x: number, y: number, bw: number, bh: number, gap: number, private reduced: boolean) {
    for (let i = 0; i < 4; i++) {
      const bx = x + (i % 2) * (bw + gap), by = y + Math.floor(i / 2) * (bh + gap);
      const frame = new Frame(scene, 0, 0, bw, bh, "btn");
      const icon = scene.add.image(30, bh / 2 - 1, iconTexture(scene, "attack")).setScale(2);
      const label = scene.add.text(58, bh / 2 - 12, "", textStyle(20)).setOrigin(0, 0.5).setY(bh / 2 - (bh > 50 ? 7 : 0));
      const sub = scene.add.text(58, bh / 2 + 11, "", textStyle(13, "#6A5A48")).setOrigin(0, 0.5);
      const key = scene.add.text(bw - 12, 8, String(i + 1), textStyle(12, C.paperDark)).setOrigin(1, 0);
      const box = scene.add.container(bx, by, [frame, icon, label, sub, key]).setDepth(32);
      const zone = scene.add.zone(bx, by, bw, bh).setOrigin(0, 0).setDepth(33).setInteractive({ useHandCursor: true });
      zone.on("pointerover", () => { if (this.active && this.buttons[i]!.enabled) this.select(i); });
      zone.on("pointerdown", () => {
        if (!this.active) return;
        this.select(i);
        this.confirm();
      });
      this.buttons.push({ frame, icon, label, sub, key, zone, box, enabled: true });
    }
  }

  setOptions(options: MenuOption[]): void {
    options.forEach((o, i) => {
      const b = this.buttons[i]!;
      b.enabled = o.enabled;
      b.icon.setTexture(iconTexture(this.scene, o.icon));
      b.label.setText(o.label);
      b.sub.setText(o.sub ?? "");
      b.label.setY(o.sub ? b.frame.fh / 2 - 8 : b.frame.fh / 2);
    });
    if (!this.buttons[this.index]!.enabled) this.index = this.buttons.findIndex((b) => b.enabled);
    if (this.index < 0) this.index = 0;
    this.refresh();
  }

  setActive(on: boolean): void {
    this.active = on;
    this.refresh();
  }

  select(i: number): void {
    if (i < 0 || i > 3) return;
    if (i !== this.index) {
      this.index = i;
      this.refresh();
      const b = this.buttons[i]!;
      if (!this.reduced && b.enabled) {
        this.scene.tweens.killTweensOf(b.icon);
        b.icon.setAngle(0);
        this.scene.tweens.add({ targets: b.icon, angle: { from: -12, to: 0 }, duration: 220, ease: "Back.easeOut" });
      }
    }
  }

  /** Moves the cursor in the 2x2 grid. */
  move(dx: number, dy: number): void {
    const col = this.index % 2, row = Math.floor(this.index / 2);
    const nc = Math.min(1, Math.max(0, col + dx)), nr = Math.min(1, Math.max(0, row + dy));
    this.select(nr * 2 + nc);
  }

  confirm(): void {
    if (!this.active) return;
    const b = this.buttons[this.index]!;
    if (!b.enabled) {
      if (!this.reduced) this.scene.tweens.add({ targets: b.box, x: b.box.x + 4, duration: 50, yoyo: true, repeat: 2 });
      return;
    }
    if (!this.reduced) this.scene.tweens.add({ targets: b.box, y: b.box.y + 3, duration: 70, yoyo: true });
    this.onConfirm(this.index);
  }

  private refresh() {
    this.buttons.forEach((b, i) => {
      const hot = this.active && i === this.index;
      b.frame.setStyle(!b.enabled ? "btnOff" : hot ? "btnHot" : "btn");
      b.box.setAlpha(this.active ? 1 : 0.75);
      b.icon.setAlpha(b.enabled ? 1 : 0.45);
      b.label.setColor(b.enabled ? C.ink : C.inkSoft);
      b.key.setColor(hot ? C.crimson : C.paperDark);
    });
  }
}

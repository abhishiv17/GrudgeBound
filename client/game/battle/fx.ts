/** Battle effects: paper-tear transition, hits, ink slashes, sparks and the binding rite. */
import * as Phaser from "phaser";
import { C, hexToInt, noise } from "./pixels";
import { speckTexture, stampTexture, talismanTexture, waxSealTexture } from "./art";
import { textStyle } from "./ui";

export function tween(scene: Phaser.Scene, config: Phaser.Types.Tweens.TweenBuilderConfig): Promise<void> {
  return new Promise((resolve) => {
    scene.tweens.add({ ...config, onComplete: () => resolve() });
  });
}

export function wait(scene: Phaser.Scene, ms: number): Promise<void> {
  return new Promise((resolve) => scene.time.delayedCall(ms, () => resolve()));
}

// ------------------------------------------------------------------ paper tear

/**
 * A sheet of paper over the whole screen, sealed with wax. It tears along a ragged diagonal
 * to reveal the battle, and closes again (the halves meet, the seal stamps down) to leave it.
 * With reduced motion it simply fades.
 */
export class PaperTear {
  private top: Phaser.GameObjects.Graphics;
  private bottom: Phaser.GameObjects.Graphics;
  private seal: Phaser.GameObjects.Image;
  private readonly w: number;
  private readonly h: number;

  constructor(private scene: Phaser.Scene, private reduced: boolean) {
    this.w = scene.scale.width;
    this.h = scene.scale.height;
    const seam = this.seam();
    this.top = this.piece([[-30, -30], [this.w + 30, -30], ...[...seam].reverse()], seam);
    this.bottom = this.piece([...seam, [this.w + 30, this.h + 30], [-30, this.h + 30]], seam);
    this.seal = scene.add.image(this.w / 2, this.h / 2, waxSealTexture(scene)).setScale(5).setDepth(1001);
  }

  private seam(): [number, number][] {
    const pts: [number, number][] = [];
    const steps = 40;
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      const x = -30 + t * (this.w + 60);
      const y = this.h * 0.64 - t * this.h * 0.28 + (noise(i, 3, 77) - 0.5) * 22 + (i % 2 ? 5 : -5);
      pts.push([Math.round(x), Math.round(y)]);
    }
    return pts;
  }

  private piece(poly: [number, number][], seam: [number, number][]): Phaser.GameObjects.Graphics {
    const g = this.scene.add.graphics().setDepth(1000);
    g.fillStyle(hexToInt(C.paper), 1);
    g.fillPoints(poly.map(([x, y]) => new Phaser.Geom.Point(x, y)), true);
    // fibres and specks, only where this piece has paper
    const inside = (x: number, y: number) => Phaser.Geom.Polygon.Contains(new Phaser.Geom.Polygon(poly.flat()), x, y);
    for (let i = 0; i < 420; i++) {
      const x = Math.floor(noise(i, 1, 91) * this.w), y = Math.floor(noise(i, 2, 91) * this.h);
      if (!inside(x, y)) continue;
      g.fillStyle(hexToInt(i % 5 ? C.paperShade : C.paperDark), 1).fillRect(x, y, i % 3 ? 3 : 6, 3);
    }
    // a faint ruled border like a ledger page
    g.lineStyle(3, hexToInt(C.crimson), 0.35).strokeRect(24, 24, this.w - 48, this.h - 48);
    // torn, fibrous white edge with an ink shadow
    g.lineStyle(10, hexToInt(C.white), 1).strokePoints(seam.map(([x, y]) => new Phaser.Geom.Point(x, y)));
    g.lineStyle(2, hexToInt(C.paperDark), 1).strokePoints(seam.map(([x, y]) => new Phaser.Geom.Point(x, y + 5)));
    return g;
  }

  /** Starts covered, then tears open. */
  async open(): Promise<void> {
    this.setCovered();
    if (this.reduced) {
      await tween(this.scene, { targets: [this.top, this.bottom, this.seal], alpha: 0, duration: 300 });
      return this.hide();
    }
    await tween(this.scene, { targets: this.seal, scale: 6, duration: 160, ease: "Quad.easeOut" });
    void tween(this.scene, { targets: this.seal, scale: 8, alpha: 0, angle: 25, duration: 260 });
    await Promise.all([
      tween(this.scene, { targets: this.top, x: -60, y: -this.h * 0.85, angle: -5, duration: 700, ease: "Cubic.easeIn" }),
      tween(this.scene, { targets: this.bottom, x: 60, y: this.h * 0.85, angle: 4, duration: 700, ease: "Cubic.easeIn" }),
    ]);
    this.hide();
  }

  /** Halves slide back together and the seal stamps down. */
  async close(): Promise<void> {
    for (const g of [this.top, this.bottom, this.seal]) g.setVisible(true);
    if (this.reduced) {
      this.setCovered();
      for (const g of [this.top, this.bottom, this.seal]) g.setAlpha(0);
      await tween(this.scene, { targets: [this.top, this.bottom, this.seal], alpha: 1, duration: 300 });
      return;
    }
    this.top.setPosition(-60, -this.h * 0.85).setAngle(-5).setAlpha(1);
    this.bottom.setPosition(60, this.h * 0.85).setAngle(4).setAlpha(1);
    this.seal.setAlpha(0).setScale(9).setAngle(0);
    await Promise.all([
      tween(this.scene, { targets: [this.top, this.bottom], x: 0, y: 0, angle: 0, duration: 520, ease: "Cubic.easeOut" }),
    ]);
    await tween(this.scene, { targets: this.seal, alpha: 1, scale: 5, duration: 200, ease: "Back.easeOut" });
    await wait(this.scene, 220);
  }

  private setCovered() {
    for (const g of [this.top, this.bottom]) g.setVisible(true).setAlpha(1).setPosition(0, 0).setAngle(0);
    this.seal.setVisible(true).setAlpha(1).setScale(5).setAngle(0);
  }

  private hide() {
    for (const g of [this.top, this.bottom, this.seal]) g.setVisible(false);
  }
}

// ------------------------------------------------------------------ hits

export function hitFlash(scene: Phaser.Scene, sprite: Phaser.GameObjects.Image, reduced: boolean): Promise<void> {
  const flashes = reduced ? 1 : 2;
  return new Promise((resolve) => {
    let n = 0;
    const on = () => {
      sprite.setTintFill(0xfff8e8);
      scene.time.delayedCall(reduced ? 120 : 60, () => {
        sprite.clearTint();
        n += 1;
        if (n < flashes) scene.time.delayedCall(60, on);
        else resolve();
      });
    };
    on();
  });
}

export function shake(scene: Phaser.Scene, reduced: boolean, strong = false): void {
  if (reduced) return;
  scene.cameras.main.shake(strong ? 260 : 170, strong ? 0.012 : 0.006);
}

/** Three brush strokes across the target in the attacker's emotion colour. */
export function slash(scene: Phaser.Scene, x: number, y: number, color: string, flip: boolean): void {
  const g = scene.add.graphics().setDepth(50);
  const c = hexToInt(color), s = flip ? -1 : 1;
  for (let i = 0; i < 3; i++) {
    const oy = (i - 1) * 18;
    g.fillStyle(hexToInt(C.ink), 1).fillTriangle(x - 46 * s, y + oy - 26, x + 44 * s, y + oy + 14, x + 40 * s, y + oy + 20);
    g.fillStyle(c, 1).fillTriangle(x - 44 * s, y + oy - 24, x + 40 * s, y + oy + 12, x + 38 * s, y + oy + 16);
  }
  scene.tweens.add({ targets: g, alpha: 0, duration: 320, delay: 90, onComplete: () => g.destroy() });
}

export function damageNumber(scene: Phaser.Scene, x: number, y: number, text: string, color: string, reduced: boolean): void {
  const t = scene.add.text(x, y, text, { ...textStyle(30, color), stroke: C.paper, strokeThickness: 6 }).setOrigin(0.5).setDepth(60);
  scene.tweens.add({
    targets: t, y: reduced ? y : y - 38, alpha: { from: 1, to: 0 }, duration: 900, ease: "Cubic.easeOut", delay: 200,
    onComplete: () => t.destroy(),
  });
}

/** Little squares flying outward (sparks, ink, paper flecks). */
export function burst(scene: Phaser.Scene, x: number, y: number, colors: string[], count: number, spread = 70, reduced = false): void {
  if (reduced) count = Math.ceil(count / 3);
  const key = speckTexture(scene);
  for (let i = 0; i < count; i++) {
    const a = (i / count) * Math.PI * 2 + noise(i, count, 5) * 0.8;
    const d = spread * (0.5 + noise(i, 7, 9) * 0.6);
    const s = scene.add.image(x, y, key).setTint(hexToInt(colors[i % colors.length]!)).setScale(1 + (i % 3)).setDepth(55);
    scene.tweens.add({
      targets: s, x: x + Math.cos(a) * d, y: y + Math.sin(a) * d + (reduced ? 0 : 20), alpha: 0, angle: 90,
      duration: 520 + (i % 4) * 60, ease: "Quad.easeOut", onComplete: () => s.destroy(),
    });
  }
}

// ------------------------------------------------------------------ the binding rite

export interface SealTarget {
  /** Where the talisman starts (near the player's curse). */
  fromX: number; fromY: number;
  /** Centre of the wild curse. */
  x: number; y: number;
  sprite: Phaser.GameObjects.Image;
  gilded: boolean;
  success: boolean;
  reduced: boolean;
}

/**
 * Talisman flies to the curse, a ring of wax seals tightens around it, the stamp presses down;
 * then either the curse folds into the talisman (success) or the talisman tears in two (failure).
 */
export async function sealRite(scene: Phaser.Scene, t: SealTarget): Promise<void> {
  const talisman = scene.add.image(t.fromX, t.fromY, talismanTexture(scene, t.gilded)).setScale(3).setDepth(40).setAngle(-30);
  const fly = { p: 0 };
  const cx = (t.fromX + t.x) / 2, cy = Math.min(t.fromY, t.y) - 120;
  await tween(scene, {
    targets: fly, p: 1, duration: t.reduced ? 250 : 560, ease: "Sine.easeInOut",
    onUpdate: () => {
      const u = fly.p, v = 1 - u;
      talisman.setPosition(v * v * t.fromX + 2 * v * u * cx + u * u * t.x, v * v * t.fromY + 2 * v * u * cy + u * u * t.y);
      if (!t.reduced) talisman.setAngle(-30 + u * 390);
    },
  });
  talisman.setAngle(0);
  if (!t.reduced) await tween(scene, { targets: talisman, scaleX: 3.6, scaleY: 2.6, duration: 80, yoyo: true });

  // ring of wax seals closing in
  const ring: Phaser.GameObjects.Image[] = [];
  const n = 8;
  for (let i = 0; i < n; i++) ring.push(scene.add.image(t.x, t.y, waxSealTexture(scene)).setScale(1.5).setDepth(41).setAlpha(0));
  const orbit = { a: 0, r: 150 };
  await tween(scene, {
    targets: orbit, a: t.reduced ? 0 : Math.PI * 1.5, r: 70, duration: t.reduced ? 400 : 1000, ease: "Sine.easeInOut",
    onUpdate: (tw) => {
      ring.forEach((s, i) => {
        const ang = orbit.a + (i / n) * Math.PI * 2;
        s.setPosition(t.x + Math.cos(ang) * orbit.r, t.y + Math.sin(ang) * orbit.r * 0.55).setAlpha(Math.min(1, tw.progress * 3));
      });
    },
  });

  // the stamp comes down
  const stamp = scene.add.image(t.x, t.y - 260, stampTexture(scene)).setScale(3).setOrigin(0.5, 1).setDepth(45);
  await tween(scene, { targets: stamp, y: t.y + 8, duration: t.reduced ? 200 : 300, ease: "Quad.easeIn" });
  const wax = scene.add.image(t.x, t.y + 6, waxSealTexture(scene)).setScale(t.reduced ? 2.5 : 4).setDepth(42);
  if (!t.reduced) {
    shake(scene, false);
    burst(scene, t.x, t.y + 10, [C.crimson, C.crimsonDark, C.paper], 14, 60);
    void tween(scene, { targets: stamp, scaleY: 2.6, duration: 70, yoyo: true });
    await tween(scene, { targets: wax, scale: 2.5, duration: 160, ease: "Back.easeOut" });
  }
  await tween(scene, { targets: stamp, y: t.y - 280, alpha: 0, duration: 320, ease: "Quad.easeOut" });
  stamp.destroy();
  await wait(scene, t.reduced ? 150 : 350);

  if (t.success) {
    ring.forEach((s) => scene.tweens.add({ targets: s, x: t.x, y: t.y, alpha: 0, scale: 0.5, duration: 320 }));
    await tween(scene, { targets: t.sprite, scaleX: 0, scaleY: 0, alpha: 0, duration: t.reduced ? 250 : 520, ease: "Back.easeIn" });
    talisman.setTint(hexToInt(C.goldLight));
    burst(scene, t.x, t.y, [C.gold, C.goldLight, C.white], 20, 90, t.reduced);
    await tween(scene, {
      targets: [talisman, wax], y: `+=${t.reduced ? 0 : 60}`, angle: t.reduced ? 0 : 8, duration: 600, ease: "Sine.easeOut",
    });
    await wait(scene, 250);
    await tween(scene, { targets: [talisman, wax], alpha: 0, duration: 400 });
  } else {
    ring.forEach((s, i) => scene.tweens.add({ targets: s, x: s.x + (i % 2 ? 40 : -40), y: s.y + 60, alpha: 0, duration: 420 }));
    const key = talismanTexture(scene, t.gilded);
    const top = scene.add.image(talisman.x, talisman.y, key).setScale(3).setDepth(40).setCrop(0, 0, 14, 15);
    const bottom = scene.add.image(talisman.x, talisman.y, key).setScale(3).setDepth(40).setCrop(0, 15, 14, 17);
    talisman.destroy();
    burst(scene, t.x, t.y, [C.paper, C.paperShade, C.white], 16, 80, t.reduced);
    void tween(scene, { targets: wax, alpha: 0, scale: 1.5, duration: 300 });
    if (!t.reduced) {
      void tween(scene, { targets: t.sprite, x: t.sprite.x + 8, duration: 50, yoyo: true, repeat: 4 });
      void hitFlash(scene, t.sprite, false);
    }
    await Promise.all([
      tween(scene, { targets: top, x: top.x - 50, y: top.y - 30, angle: -40, alpha: 0, duration: 650, ease: "Quad.easeOut" }),
      tween(scene, { targets: bottom, x: bottom.x + 40, y: bottom.y + 80, angle: 35, alpha: 0, duration: 650, ease: "Quad.easeIn" }),
    ]);
    top.destroy();
    bottom.destroy();
  }
  ring.forEach((s) => s.destroy());
  wax.destroy();
  talisman.destroy();
}

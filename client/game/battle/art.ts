/**
 * Generated UI textures for the battle screen: 9-slice paper frames, icons, the talisman,
 * wax seal, stamp, Kessa and paper grain. All drawn in code, no image files, no lettering.
 *
 * Technique notes (ideas only, no code or assets copied): the framed panels follow the
 * "border image" idea used by RPG web UI kits (ornamented corners that never stretch, plain
 * edges that do), and the stepped pixel corners mimic the notched borders of pixel-style CSS
 * kits. Here they are drawn as small textures and assembled by `frame()` below.
 */
import * as Phaser from "phaser";
import { Pixels, C, makePixelTexture, makeTexture, noise } from "./pixels";

// ------------------------------------------------------------------ 9-slice frames

export type FrameStyle = "panel" | "log" | "card" | "btn" | "btnHot" | "btnOff";
const FRAME_SRC = 24;
const FRAME_CORNER = 8;

interface FrameLook {
  layers: { inset: number; color: string; cut: number }[];
  ornament?: string;
  shadow?: string; // thicker bottom edge, for raised buttons
}

const LOOKS: Record<FrameStyle, FrameLook> = {
  panel: {
    layers: [
      { inset: 0, color: C.ink, cut: 3 }, { inset: 2, color: C.gold, cut: 2 },
      { inset: 3, color: C.paperDark, cut: 1 }, { inset: 4, color: C.paper, cut: 0 },
    ],
    ornament: C.crimson,
  },
  log: {
    layers: [
      { inset: 0, color: "#4A3020", cut: 3 }, { inset: 2, color: "#9C7A4E", cut: 2 },
      { inset: 3, color: "#E4D2A6", cut: 1 }, { inset: 4, color: "#EADCB6", cut: 0 },
    ],
  },
  card: {
    layers: [
      { inset: 0, color: C.ink, cut: 3 }, { inset: 2, color: C.paperDark, cut: 1 }, { inset: 3, color: C.paper, cut: 0 },
    ],
    ornament: C.gold,
  },
  btn: {
    layers: [
      { inset: 0, color: C.ink, cut: 3 }, { inset: 2, color: C.white, cut: 1 }, { inset: 3, color: C.paper, cut: 0 },
    ],
    shadow: C.paperDark,
  },
  btnHot: {
    layers: [
      { inset: 0, color: C.ink, cut: 3 }, { inset: 2, color: C.gold, cut: 1 }, { inset: 4, color: "#FFF4D8", cut: 0 },
    ],
    shadow: C.goldDark,
    ornament: C.crimson,
  },
  btnOff: {
    layers: [
      { inset: 0, color: C.inkSoft, cut: 3 }, { inset: 2, color: C.paperDark, cut: 1 }, { inset: 3, color: C.paperShade, cut: 0 },
    ],
  },
};

function paintFrame(look: FrameLook): Pixels {
  const n = FRAME_SRC, p = new Pixels(n, n);
  for (const { inset, color, cut } of look.layers) {
    for (let y = inset; y < n - inset; y++) for (let x = inset; x < n - inset; x++) {
      const dx = Math.min(x - inset, n - 1 - inset - x), dy = Math.min(y - inset, n - 1 - inset - y);
      if (dx + dy >= cut) p.px(x, y, color);
    }
  }
  if (look.shadow) {
    const fill = look.layers.at(-1)!.color;
    for (let x = 3; x < n - 3; x++) {
      for (let y = n - 6; y < n - 3; y++) if (p.get(x, y) === fill) p.px(x, y, y >= n - 5 ? look.shadow : fill);
    }
  }
  if (look.ornament) {
    for (const [cx, cy] of [[5, 5], [n - 6, 5], [5, n - 6], [n - 6, n - 6]] as const) {
      p.px(cx, cy - 1, look.ornament).px(cx - 1, cy, look.ornament).px(cx, cy, look.ornament).px(cx + 1, cy, look.ornament).px(cx, cy + 1, look.ornament);
    }
  }
  return p;
}

function ensureFrame(scene: Phaser.Scene, style: FrameStyle): string {
  const key = `frame-${style}`;
  if (scene.textures.exists(key)) return key;
  makePixelTexture(scene, key, paintFrame(LOOKS[style]));
  const tex = scene.textures.get(key);
  const k = FRAME_CORNER, m = FRAME_SRC - 2 * k;
  const cols = [0, k, k + m], ws = [k, m, k];
  for (let r = 0; r < 3; r++) for (let c = 0; c < 3; c++) tex.add(`s${r}${c}`, 0, cols[c]!, cols[r]!, ws[c]!, ws[r]!);
  return key;
}

/**
 * A 9-slice paper frame: corners keep their pixels, edges and centre stretch.
 * Works in both the WebGL and Canvas renderers (Phaser's NineSlice object is WebGL-only).
 * `scale` is the art-pixel size on screen.
 */
export class Frame extends Phaser.GameObjects.Container {
  private parts: Phaser.GameObjects.Image[] = [];
  constructor(scene: Phaser.Scene, x: number, y: number, public fw: number, public fh: number, private style: FrameStyle, private pixel = 2) {
    super(scene, x, y);
    for (let r = 0; r < 3; r++) for (let c = 0; c < 3; c++) {
      const img = scene.add.image(0, 0, ensureFrame(scene, style), `s${r}${c}`).setOrigin(0, 0);
      this.parts.push(img);
      this.add(img);
    }
    this.layout();
    scene.add.existing(this);
  }

  setStyle(style: FrameStyle): this {
    if (style === this.style) return this;
    this.style = style;
    const key = ensureFrame(this.scene, style);
    this.parts.forEach((img, i) => img.setTexture(key, `s${Math.floor(i / 3)}${i % 3}`));
    return this;
  }

  private layout() {
    const k = FRAME_CORNER * this.pixel;
    const mw = Math.max(0, this.fw - 2 * k), mh = Math.max(0, this.fh - 2 * k);
    const xs = [0, k, k + mw], ys = [0, k, k + mh];
    const sx = [this.pixel, mw / (FRAME_SRC - 2 * FRAME_CORNER), this.pixel];
    const sy = [this.pixel, mh / (FRAME_SRC - 2 * FRAME_CORNER), this.pixel];
    this.parts.forEach((img, i) => {
      const r = Math.floor(i / 3), c = i % 3;
      img.setPosition(xs[c]!, ys[r]!).setScale(sx[c]!, sy[r]!);
    });
  }
}

// ------------------------------------------------------------------ paper grain

/** Tileable paper fibre texture, drawn over flat paper areas at low alpha. */
export function ensureGrain(scene: Phaser.Scene): string {
  const key = "paper-grain";
  makeTexture(scene, key, 32, 32, (ctx) => {
    for (let y = 0; y < 32; y++) for (let x = 0; x < 32; x++) {
      const n = noise(x, y, 3);
      if (n < 0.06) { ctx.fillStyle = C.paperDark; ctx.fillRect(x, y, 1, 1); }
      else if (n > 0.96) { ctx.fillStyle = C.white; ctx.fillRect(x, y, 1, 1); }
    }
    ctx.fillStyle = C.paperShade;
    for (let i = 0; i < 6; i++) {
      const x = Math.floor(noise(i, 9, 4) * 28), y = Math.floor(noise(i, 2, 5) * 32);
      ctx.fillRect(x, y, 3 + (i % 3), 1);
    }
  });
  return key;
}

// ------------------------------------------------------------------ icons (16x16)

export type IconId = "attack" | "ward" | "seal" | "item" | "salve" | "gilded" | "flee" | "back";

const ICONS: Record<IconId, (p: Pixels) => void> = {
  attack: (p) => {
    p.poly([[3, 12], [11, 2], [14, 5], [6, 15]], C.paper);
    p.outline();
    p.line(5, 12, 12, 4, C.crimson).line(6, 13, 13, 5, C.crimson).px(9, 9, C.paper).px(8, 9, C.crimsonDark);
    for (const [x, y] of [[1, 2], [2, 6], [6, 0], [14, 11], [11, 14]] as const) p.px(x, y, C.rageLight);
    p.px(0, 4, C.ember).px(15, 14, C.ember);
  },
  ward: (p) => {
    p.poly([[2, 2], [13, 2], [13, 9], [7.5, 15], [2, 9]], C.paper);
    p.outline();
    p.recolor(C.paper, C.paperShade, (x) => x > 8);
    p.ring(7.5, 7, 3, 1, C.sorrow).px(7, 7, C.sorrowLight).px(8, 7, C.sorrowLight);
  },
  seal: (p) => {
    p.disc(8, 7, 6, C.crimson).rect(6, 12, 2, 3, C.crimson).rect(10, 12, 2, 2, C.crimson);
    p.outline();
    p.ring(8, 7, 4, 1, C.crimsonLight).px(8, 7, C.crimsonDark).px(7, 6, C.crimsonDark).px(9, 8, C.crimsonDark).px(8, 5, C.crimsonDark).px(8, 9, C.crimsonDark);
  },
  item: (p) => {
    p.ellipse(8, 10, 6, 5, C.woodLight).rect(5, 3, 6, 3, C.woodLight);
    p.outline();
    p.rect(4, 5, 8, 1, C.crimson).recolor(C.woodLight, C.wood, (x, y) => x + y > 18);
    p.disc(11, 11, 2, C.gold).px(11, 11, C.goldDark);
  },
  salve: (p) => {
    p.rect(3, 6, 10, 9, C.paperShade).rect(4, 3, 8, 3, C.gold);
    p.outline();
    p.rect(3, 9, 10, 3, C.crimson).rect(4, 13, 8, 2, C.paperDark).px(5, 4, C.goldLight);
  },
  gilded: (p) => {
    p.rect(5, 1, 6, 14, C.goldLight);
    p.outline();
    p.rect(5, 1, 6, 2, C.gold).ring(8, 8, 2, 1, C.crimson).line(8, 11, 8, 13, C.crimson).px(10, 5, C.white);
  },
  flee: (p) => {
    p.ellipse(10, 5, 3, 2, C.paper).ellipse(6, 11, 3, 2, C.paper);
    p.outline();
    p.rect(1, 3, 4, 1, C.inkSoft).rect(0, 6, 3, 1, C.inkSoft).rect(1, 10, 1, 1, C.inkSoft);
    p.px(10, 5, C.wood).px(6, 11, C.wood);
  },
  back: (p) => {
    p.poly([[2, 7], [7, 2], [7, 5], [13, 5], [13, 10], [7, 10], [7, 13]], C.paperShade);
    p.outline();
  },
};

export function iconTexture(scene: Phaser.Scene, id: IconId): string {
  const key = `icon-${id}`;
  if (!scene.textures.exists(key)) {
    const p = new Pixels(16, 16);
    ICONS[id](p);
    makePixelTexture(scene, key, p);
  }
  return key;
}

/** Emotion glyphs (9x9): Rage flame, Sorrow tear, Dread eye. */
export function emotionIcon(scene: Phaser.Scene, emotion: "rage" | "sorrow" | "dread"): string {
  const key = `emo-${emotion}`;
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(11, 11);
  if (emotion === "rage") {
    p.poly([[2, 9], [3, 4], [5, 6], [6, 1], [8, 5], [9, 9]], C.rage).ellipse(5.5, 8, 3.5, 2, C.rage);
    p.outline();
    p.ellipse(5.5, 8, 1.5, 1, C.ember);
  } else if (emotion === "sorrow") {
    p.poly([[5.5, 1], [9, 7], [2, 7]], C.sorrow).disc(5.5, 7, 3.2, C.sorrow);
    p.outline();
    p.px(4, 6, C.sorrowLight).px(4, 7, C.sorrowLight);
  } else {
    p.ellipse(5.5, 5.5, 5, 3, C.dread);
    p.outline();
    p.disc(5.5, 5.5, 1.5, C.dreadLight).px(5, 5, C.white);
  }
  makePixelTexture(scene, key, p);
  return key;
}

/** Omen pip: a tiny hanging lantern, lit or unlit. */
export function omenPip(scene: Phaser.Scene, lit: boolean): string {
  const key = lit ? "pip-lit" : "pip-dim";
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(7, 9);
  p.ellipse(3, 5, 2.5, 3, lit ? C.gold : C.paperShade).px(3, 1, C.ink);
  p.outline();
  p.rect(1, 2, 5, 1, C.crimson).px(3, 5, lit ? C.goldLight : C.paperDark);
  makePixelTexture(scene, key, p);
  return key;
}

// ------------------------------------------------------------------ binding props

export function talismanTexture(scene: Phaser.Scene, gilded: boolean): string {
  const key = gilded ? "talisman-gilded" : "talisman";
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(14, 32);
  const base = gilded ? C.goldLight : C.paper;
  p.rect(2, 1, 10, 29, base);
  p.outline();
  p.rect(2, 1, 10, 3, gilded ? C.gold : C.crimson).rect(2, 27, 10, 3, gilded ? C.gold : C.crimson);
  // abstract sigil: ring, spine and ticks (deliberately not a letter)
  p.ring(7, 10, 3, 1, C.crimson).line(7, 13, 7, 23, C.ink).line(4, 16, 6, 18, C.ink).line(10, 16, 8, 18, C.ink);
  p.rect(5, 21, 5, 1, C.crimson).px(7, 10, C.ink);
  p.recolor(base, gilded ? C.gold : C.paperShade, (x) => x === 11);
  makePixelTexture(scene, key, p);
  return key;
}

export function waxSealTexture(scene: Phaser.Scene): string {
  const key = "wax-seal";
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(20, 22);
  p.disc(10, 10, 8, C.crimson).ellipse(4, 16, 2, 3, C.crimson).ellipse(15, 17, 1.5, 3, C.crimson).disc(10, 18, 2, C.crimson);
  p.outline(C.crimsonDark);
  p.ring(10, 10, 6, 1, C.crimsonLight);
  for (const [x, y] of [[10, 6], [10, 14], [6, 10], [14, 10], [10, 10], [8, 8], [12, 12], [12, 8], [8, 12]] as const) p.px(x, y, C.crimsonDark);
  p.px(7, 5, C.white).px(6, 6, C.crimsonLight);
  makePixelTexture(scene, key, p);
  return key;
}

export function stampTexture(scene: Phaser.Scene): string {
  const key = "stamp-tool";
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(20, 28);
  p.disc(10, 5, 4, C.woodLight).rect(8, 8, 4, 9, C.wood).rect(3, 17, 14, 5, C.woodLight).rect(2, 22, 16, 4, C.gold);
  p.outline();
  p.px(8, 3, C.paper).rect(3, 20, 14, 1, C.wood).rect(2, 25, 16, 1, C.goldDark).rect(2, 26, 16, 1, C.crimson);
  makePixelTexture(scene, key, p);
  return key;
}

/** Kessa, the paper-lantern referee spirit (floats above the fight, not a combatant). */
export function kessaTexture(scene: Phaser.Scene): string {
  const key = "kessa";
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(26, 34);
  p.line(13, 0, 13, 4, C.ink);
  p.ellipse(13, 14, 10, 9, C.paper);
  p.rect(8, 4, 10, 2, C.crimson).rect(8, 22, 10, 2, C.crimson);
  p.line(13, 24, 13, 30, C.crimson).rect(12, 30, 3, 3, C.crimson);
  p.rect(1, 12, 3, 8, C.paper).rect(22, 10, 3, 8, C.paper);
  p.outline();
  p.recolor(C.paper, C.goldLight, (x, y) => Math.hypot(x - 13, y - 14) < 5);
  p.recolor(C.paper, C.paperShade, (x, y) => (x === 7 || x === 19) && y > 6 && y < 22);
  p.rect(9, 12, 2, 2, C.ink).rect(16, 12, 2, 2, C.ink).px(9, 12, C.white).px(16, 12, C.white);
  p.rect(11, 17, 5, 1, C.ink).px(10, 16, C.ink).px(16, 16, C.ink);
  p.px(8, 15, C.crimsonLight).px(18, 15, C.crimsonLight);
  p.rect(2, 14, 1, 3, C.crimson).rect(23, 12, 1, 3, C.crimson);
  makePixelTexture(scene, key, p);
  return key;
}

/** 3x3 particle used for sparks, ink drops and paper flecks (tinted per use). */
export function speckTexture(scene: Phaser.Scene): string {
  const key = "speck";
  makeTexture(scene, key, 3, 3, (ctx) => { ctx.fillStyle = "#FFFFFF"; ctx.fillRect(0, 0, 3, 3); });
  return key;
}

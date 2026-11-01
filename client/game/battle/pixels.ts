/**
 * Tiny pixel painter for procedural art. Shapes are painted into a colour grid, an automatic
 * ink outline can be traced around the silhouette, and the grid is then copied 1:1 into a
 * Phaser canvas texture (scaled up later with nearest-neighbour, so pixels stay crisp).
 */
import type * as Phaser from "phaser";

export const C = {
  ink: "#1A1726",
  inkSoft: "#3A3350",
  paper: "#F3E9D2",
  paperShade: "#DCCBA6",
  paperDark: "#B9A47C",
  white: "#FFF8E8",
  gold: "#D9A532",
  goldDark: "#9C7019",
  goldLight: "#F2D27A",
  crimson: "#B0153A",
  crimsonDark: "#7A0E28",
  crimsonLight: "#E0506A",
  rage: "#C8402A",
  rageDark: "#8A2A1A",
  rageLight: "#F0884B",
  ember: "#FFD27A",
  sorrow: "#2B6CB0",
  sorrowDark: "#1B4577",
  sorrowLight: "#7FB2E5",
  dread: "#6B46C1",
  dreadDark: "#43297F",
  dreadLight: "#A78BEA",
  wax: "#EEE2C4",
  waxShade: "#CDBF9C",
  wood: "#5E3A22",
  woodLight: "#8A5A35",
  iron: "#4C4A5C",
  ironLight: "#7D7B90",
  mouth: "#3B0A16",
  shadow: "#2A2340",
} as const;

export type Color = string;

export class Pixels {
  readonly grid: (Color | null)[];
  constructor(readonly w: number, readonly h: number) {
    this.grid = new Array<Color | null>(w * h).fill(null);
  }

  get(x: number, y: number): Color | null {
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return null;
    return this.grid[y * this.w + x] ?? null;
  }

  px(x: number, y: number, c: Color | null): this {
    x = Math.round(x); y = Math.round(y);
    if (x >= 0 && y >= 0 && x < this.w && y < this.h) this.grid[y * this.w + x] = c;
    return this;
  }

  rect(x: number, y: number, w: number, h: number, c: Color | null): this {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) this.px(x + i, y + j, c);
    return this;
  }

  /** Filled ellipse centred on (cx, cy) with radii rx, ry. */
  ellipse(cx: number, cy: number, rx: number, ry: number, c: Color | null): this {
    for (let y = Math.floor(cy - ry); y <= Math.ceil(cy + ry); y++) {
      for (let x = Math.floor(cx - rx); x <= Math.ceil(cx + rx); x++) {
        const dx = (x - cx) / (rx + 0.35), dy = (y - cy) / (ry + 0.35);
        if (dx * dx + dy * dy <= 1) this.px(x, y, c);
      }
    }
    return this;
  }

  disc(cx: number, cy: number, r: number, c: Color | null): this {
    return this.ellipse(cx, cy, r, r, c);
  }

  /** Ring of thickness t. */
  ring(cx: number, cy: number, r: number, t: number, c: Color): this {
    for (let y = Math.floor(cy - r); y <= Math.ceil(cy + r); y++) {
      for (let x = Math.floor(cx - r); x <= Math.ceil(cx + r); x++) {
        const d = Math.hypot(x - cx, y - cy);
        if (d <= r + 0.35 && d > r - t + 0.35) this.px(x, y, c);
      }
    }
    return this;
  }

  line(x0: number, y0: number, x1: number, y1: number, c: Color): this {
    const steps = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1);
    for (let i = 0; i <= steps; i++) this.px(x0 + ((x1 - x0) * i) / steps, y0 + ((y1 - y0) * i) / steps, c);
    return this;
  }

  /** Filled convex or simple polygon (even-odd scanline fill). */
  poly(points: readonly [number, number][], c: Color): this {
    const ys = points.map((p) => p[1]);
    for (let y = Math.floor(Math.min(...ys)); y <= Math.ceil(Math.max(...ys)); y++) {
      const xs: number[] = [];
      for (let i = 0; i < points.length; i++) {
        const [ax, ay] = points[i]!;
        const [bx, by] = points[(i + 1) % points.length]!;
        const sy = y + 0.5;
        if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) xs.push(ax + ((sy - ay) / (by - ay)) * (bx - ax));
      }
      xs.sort((a, b) => a - b);
      for (let k = 0; k + 1 < xs.length; k += 2) {
        for (let x = Math.round(xs[k]!); x < Math.round(xs[k + 1]!); x++) this.px(x, y, c);
      }
    }
    return this;
  }

  /** Recolour every pixel of colour `from` that passes `where`. Handy for simple shading. */
  recolor(from: Color, to: Color, where: (x: number, y: number) => boolean): this {
    for (let y = 0; y < this.h; y++) for (let x = 0; x < this.w; x++) {
      if (this.get(x, y) === from && where(x, y)) this.px(x, y, to);
    }
    return this;
  }

  /** Paints `c` on every empty pixel that touches a filled one (4-neighbour): a clean 1px outline. */
  outline(c: Color = C.ink): this {
    const add: number[] = [];
    for (let y = 0; y < this.h; y++) for (let x = 0; x < this.w; x++) {
      if (this.get(x, y) !== null) continue;
      if (this.get(x - 1, y) || this.get(x + 1, y) || this.get(x, y - 1) || this.get(x, y + 1)) add.push(y * this.w + x);
    }
    for (const i of add) this.grid[i] = c;
    return this;
  }

  /** Copies the grid into a canvas context at (ox, oy). */
  draw(ctx: CanvasRenderingContext2D, ox = 0, oy = 0): void {
    for (let y = 0; y < this.h; y++) {
      let x = 0;
      while (x < this.w) {
        const c = this.grid[y * this.w + x];
        if (!c) { x++; continue; }
        let run = 1;
        while (x + run < this.w && this.grid[y * this.w + x + run] === c) run++;
        ctx.fillStyle = c;
        ctx.fillRect(ox + x, oy + y, run, 1);
        x += run;
      }
    }
  }
}

/** Creates (or replaces) a canvas texture of size w x h and lets `paint` draw into it. */
export function makeTexture(
  scene: Phaser.Scene, key: string, w: number, h: number,
  paint: (ctx: CanvasRenderingContext2D) => void,
): void {
  if (scene.textures.exists(key)) return;
  const tex = scene.textures.createCanvas(key, w, h);
  if (!tex) return;
  const ctx = tex.getContext();
  ctx.imageSmoothingEnabled = false;
  paint(ctx);
  tex.refresh();
}

export function makePixelTexture(scene: Phaser.Scene, key: string, p: Pixels): void {
  makeTexture(scene, key, p.w, p.h, (ctx) => p.draw(ctx));
}

/** Small deterministic hash noise for texture grain (no Math.random, so art is identical every run). */
export function noise(x: number, y: number, seed = 0): number {
  let h = (x * 374761393 + y * 668265263 + seed * 2147483647) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

export function hexToInt(hex: string): number {
  return parseInt(hex.replace("#", ""), 16);
}

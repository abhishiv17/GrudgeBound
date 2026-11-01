/**
 * Tiny pixel maps shared by Phaser (canvas textures) and React (inline SVG icons).
 * Each row is a string; each character is a palette key, "." is transparent.
 * `outline: true` adds a 1px ink outline around the opaque pixels, so maps stay readable.
 * Colors follow docs/ART_PROMPTS.md section 2.
 */

export const INK = "#1A1726";

export interface PixelMap {
  rows: readonly string[];
  palette: Readonly<Record<string, string>>;
  outline?: boolean;
}

export type Grid = (string | null)[][];

/** Adds a 1px ink outline around the opaque pixels of a grid (grid size unchanged). */
export function outlineGrid(grid: Grid): Grid {
  const out = grid.map((r) => [...r]);
  for (let y = 0; y < grid.length; y++) {
    for (let x = 0; x < grid[y]!.length; x++) {
      if (grid[y]![x]) continue;
      const near = [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([dx, dy]) => grid[y + dy!]?.[x + dx!]);
      if (near) out[y]![x] = INK;
    }
  }
  return out;
}

/** Returns a grid of colors (null = transparent), with the optional outline already applied. */
export function rasterize(map: PixelMap, overrides: Record<string, string> = {}): Grid {
  const pad = map.outline ? 1 : 0;
  const h = map.rows.length;
  const w = Math.max(...map.rows.map((r) => r.length));
  const grid: Grid = Array.from({ length: h + pad * 2 }, () => Array<string | null>(w + pad * 2).fill(null));
  for (let y = 0; y < h; y++) {
    const row = map.rows[y]!;
    for (let x = 0; x < row.length; x++) {
      const ch = row[x]!;
      if (ch === ".") continue;
      const color = overrides[ch] ?? map.palette[ch];
      if (color) grid[y + pad]![x + pad] = color;
    }
  }
  return map.outline ? outlineGrid(grid) : grid;
}

export function mapSize(map: PixelMap): { w: number; h: number } {
  const pad = map.outline ? 2 : 0;
  return { w: Math.max(...map.rows.map((r) => r.length)) + pad, h: map.rows.length + pad };
}

/** Draws a pixel map onto a 2D canvas at (ox, oy), `scale` canvas pixels per map pixel. */
export function drawPixelMap(ctx: CanvasRenderingContext2D, map: PixelMap, ox: number, oy: number, scale = 1, overrides: Record<string, string> = {}) {
  drawGrid(ctx, rasterize(map, overrides), ox, oy, scale);
}

/** Paints a color grid onto a 2D canvas. */
export function drawGrid(ctx: CanvasRenderingContext2D, grid: Grid, ox: number, oy: number, scale = 1) {
  for (let y = 0; y < grid.length; y++) {
    for (let x = 0; x < grid[y]!.length; x++) {
      const c = grid[y]![x];
      if (!c) continue;
      ctx.fillStyle = c;
      ctx.fillRect(ox + x * scale, oy + y * scale, scale, scale);
    }
  }
}

// ----------------------------------------------------------------------------- Kessa

/** Kessa, the paper-lantern referee: round lantern body, happy face, red tassel. */
export const KESSA: PixelMap = {
  outline: true,
  palette: {
    c: "#4A2E1E", C: "#7A4E2E", g: "#D9A532", y: "#FFF4D2", p: "#FBE3A6",
    o: "#E9A54B", O: "#C8742A", e: INK, m: "#7E0F2A", b: "#EF8A78", r: "#B0153A", R: "#7E0F2A",
  },
  rows: [
    "......cCCCCc......",
    ".....gggggggg.....",
    "....oooooooooo....",
    "...oppppyyppppo...",
    "..opppyyyyyypppo..",
    ".oooooooooooooooo.",
    ".ppyyyyyyyyyyyypp.",
    ".pyyyyyyyyyyyyyyp.",
    ".pyyyeyyyyyyeyyyp.",
    ".pyyyeyyyyyyeyyyp.",
    ".pybbyymyymyybbyp.",
    ".pyyyyyymmyyyyyyp.",
    ".oooooooooooooooo.",
    "..oppppppppppppo..",
    "...oooppppppooo...",
    "....OOOOOOOOOO....",
    ".....gggggggg.....",
    "......cCCCCc......",
    "........rr........",
    "........rr........",
    ".......rRRr.......",
    ".......rrrr.......",
    "......rRrrRr......",
    ".......r..r.......",
  ],
};

/** Kessa with a stern face (used when the Arbiter rejects a rule). */
export const KESSA_STERN: PixelMap = {
  ...KESSA,
  rows: KESSA.rows.map((row, i) => {
    if (i === 7) return ".pyyyeyyyyyyeyyyp.";
    if (i === 8) return ".pyyyyeyyyyeyyyyp.";
    if (i === 9) return ".pyyyyeyyyyeyyyyp.";
    if (i === 10) return ".pyyyyyyyyyyyyyyp.";
    if (i === 11) return ".pyyyyymmmmyyyyyp.";
    return row;
  }),
};

// ----------------------------------------------------------------------------- icons

export const COIN: PixelMap = {
  outline: true,
  palette: { d: "#9E7420", g: "#D9A532", y: "#F6D77A" },
  rows: [
    "..ddddd..",
    ".dyyyggd.",
    "dyygggggd",
    "dyg...ggd",
    "dgg...ggd",
    "dgg...gdd",
    "dgggggggd",
    ".dggggdd.",
    "..ddddd..",
  ],
};

export const LANTERN_ICON: PixelMap = {
  outline: true,
  palette: { c: "#4A2E1E", o: "#E2672E", y: "#F7C35C", r: "#B0153A" },
  rows: [
    "..ccc..",
    ".ooooo.",
    "oyyyyyo",
    "oyyyyyo",
    "ooooooo",
    "oyyyyyo",
    ".ooooo.",
    "..ccc..",
    "...r...",
    "...r...",
  ],
};

export const MOON: PixelMap = {
  outline: true,
  palette: { M: "#F3E9D2", m: "#CFC2A3" },
  rows: [
    "..MMMM..",
    ".MMMm...",
    "MMMm....",
    "MMMm....",
    "MMMm....",
    "MMMMm...",
    ".MMMMMm.",
    "..MMMM..",
  ],
};

export const SUN: PixelMap = {
  outline: true,
  palette: { g: "#D9A532", y: "#F6D77A", o: "#E2862E" },
  rows: [
    "...oo...",
    ".oggggo.",
    ".gyyggg.",
    "ogyggggo",
    "ogggggo.",
    ".gggggg.",
    ".oggggo.",
    "...oo...",
  ],
};

export const NOTICE: PixelMap = {
  outline: true,
  palette: { p: "#F3E9D2", l: "#8A7A60", r: "#B0153A" },
  rows: [
    "ppprppp",
    "ppppppp",
    "pllllpp",
    "ppppppp",
    "plllllp",
    "ppppppp",
    "pllllpp",
    "ppppppp",
  ],
};

export const EMOTION_GLYPH: Record<"rage" | "sorrow" | "dread", PixelMap> = {
  rage: {
    outline: true,
    palette: { f: "#C8402A", y: "#F2A33A" },
    rows: ["...f...", "..ff...", "..fff.f", ".ffyff.", "ffyyyff", "ffyyyff", ".fffff."],
  },
  sorrow: {
    outline: true,
    palette: { s: "#2B6CB0", l: "#8CC0EA" },
    rows: ["...s...", "...s...", "..sss..", ".sslss.", ".slsss.", ".sssss.", "..sss.."],
  },
  dread: {
    outline: true,
    palette: { v: "#6B46C1", w: "#E9DDFB", e: INK },
    rows: ["..vvv..", ".vwwwv.", "vwwewwv", "vwweewv", ".vwwwv.", "..vvv.."],
  },
};

// ----------------------------------------------------------------------------- curse portraits

/** Portraits for the encounter dialog. "A"/"a" take the curse's emotion color (main/dark). */
export const CURSE_PORTRAIT: Record<"rage" | "sorrow" | "dread", PixelMap> = {
  // An ember-hot coin with a jagged grin.
  rage: {
    outline: true,
    palette: { A: "#C8402A", a: "#7E1E14", f: "#F2A33A", g: "#D9A532", w: "#FFF6E0", e: INK, m: "#4A1020" },
    rows: [
      ".....f..f.f.....",
      "....fAffAAf.....",
      "....AAAAAAAA....",
      "...AAggggggAA...",
      "..AAggggggggAA..",
      "..AgwwggggwwgA..",
      ".AAgweggggewgAA.",
      ".AggggggggggggA.",
      ".AgmmmmmmmmmmgA.",
      ".Agmwmwmwmwmmga.",
      ".AggmmmmmmmmggA.",
      "..AggggggggggA..",
      "..AAggggggggAA..",
      "...AAggggggAA...",
      "....aAAAAAAa....",
      "......aaaa......",
    ],
  },
  // A melting candle crying blue wax, a wage slip stuck to its side.
  sorrow: {
    outline: true,
    palette: { A: "#2B6CB0", a: "#173E6B", f: "#F2A33A", y: "#FFF4D2", k: INK, c: "#EFE3C8", C: "#CDBF9F", e: INK, m: "#5A4A3A", p: "#FFFFFF", r: "#B0153A" },
    rows: [
      ".......f........",
      "......fyf.......",
      "......fyf.......",
      ".......k........",
      ".....cccccC.....",
      "....ccccccccC...",
      "....cecccceC....",
      "....cAcccccAC...",
      "...ccAcmmccAcC..",
      "...cAccccccCAC..",
      "...cAcppppccCA..",
      "....ccprrpccC...",
      "....ccppppccC...",
      "...cccccccccCC..",
      "..CCCCCCCCCCCCC.",
      "...aaaaaaaaaaa..",
    ],
  },
  // A moth of stamped paper wings, one glowing eye on each wing.
  dread: {
    outline: true,
    palette: { A: "#6B46C1", a: "#3B2470", w: "#F3E9D2", W: "#D6C7A6", r: "#B0153A", y: "#F7C35C", e: INK, b: "#2A2538" },
    rows: [
      "...e........e...",
      "....e......e....",
      "AAA..e.bb.e..AAA",
      "AwwA..bbbb..AwwA",
      "AwywA.byyb.AwywA",
      "AwwwwAbbbbAwwwwA",
      "AwrrwAbbbbAwrrwA",
      "AwrrwwAbbAwwrrwA",
      ".AwwwwAbbAwwwwA.",
      ".AWwwAAbbAAwwWA.",
      "..AWwAabbaAwWA..",
      "..AWWA.bb.AWWA..",
      "...AA..bb..AA...",
      ".......aa.......",
      "................",
      "................",
    ],
  },
};

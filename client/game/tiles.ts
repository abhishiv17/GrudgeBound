/**
 * Lantern Ward tiles, drawn in code until the real 32x32 tileset arrives (docs/ART_PROMPTS.md, prompt D).
 * Top-down 3/4 view on a 32x32 grid (not isometric): we see the tops of things and a little of their fronts.
 * Colors follow the paper style guide: cream sandstone, warm wood, crimson awnings, teal water.
 *
 * Tile ids 0-8 keep their original meaning; later ids were added for the night-market dressing.
 */
export const TILE = 32;

export const T = {
  path: 0,
  grass: 1,
  tallGrass: 2,
  water: 3,
  wall: 4,
  stall: 5, // left half of a two-tile stall
  lantern: 6,
  board: 7,
  pier: 8,
  stallR: 9,
  plaza: 10,
  path2: 11,
  flowers: 12,
  crate: 13,
  plant: 14,
  quay: 15,
  water2: 16,
  rug: 17,
  tallGrassOver: 18,
  bannerPost: 19,
  wallBanner: 20,
  fountainTL: 21,
  fountainTR: 22,
  fountainBL: 23,
  fountainBR: 24,
} as const;

export const SOLID: number[] = [
  T.water, T.water2, T.quay, T.wall, T.wallBanner, T.stall, T.stallR, T.lantern, T.board,
  T.crate, T.plant, T.bannerPost, T.fountainTL, T.fountainTR, T.fountainBL, T.fountainBR,
];
export const TILE_COUNT = 25;

/** Palette (style guide first, then supporting tones). */
const C = {
  ink: "#1A1726",
  paper: "#F3E9D2",
  gold: "#D9A532",
  goldDark: "#9E7420",
  crimson: "#B0153A",
  crimsonDark: "#7E0F2A",
  rage: "#C8402A",
  sorrow: "#2B6CB0",
  mortar: "#A48C63",
  stone: "#E6D6B2",
  stoneLight: "#F2E6C8",
  stoneShade: "#C9B48A",
  stoneDark: "#B09A70",
  grass: "#7C9A4C",
  grassLight: "#97B35E",
  grassDark: "#5E7B38",
  tall: "#4A7232",
  tallDark: "#33532A",
  tallLight: "#8DB35E",
  water: "#2E97A3",
  waterLight: "#63C3C2",
  waterDark: "#1E6E7E",
  wood: "#7A4E2E",
  woodLight: "#A8743F",
  woodDark: "#4A2E1E",
  teal: "#3A7CA5",
  leaf: "#4F8A3C",
  leafLight: "#79B05A",
  pot: "#2F5E8C",
  potLight: "#4E86B8",
};

type Ctx = CanvasRenderingContext2D;

/** Small deterministic hash so the same tile always gets the same speckles. */
function hash(a: number, b: number): number {
  let h = (a * 374761393 + b * 668265263) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

function rect(ctx: Ctx, x: number, y: number, w: number, h: number, color: string) {
  ctx.fillStyle = color;
  ctx.fillRect(x, y, w, h);
}

function specks(ctx: Ctx, ox: number, oy: number, color: string, count: number, seed: number, size = 1) {
  ctx.fillStyle = color;
  for (let i = 0; i < count; i++) {
    const px = Math.floor(hash(seed, i * 2) * (TILE - size));
    const py = Math.floor(hash(seed, i * 2 + 1) * (TILE - size));
    ctx.fillRect(ox + px, oy + py, size, size);
  }
}

/** Rounded cobblestones with a lit top-left and a shaded bottom-right. */
function cobbles(ctx: Ctx, x: number, seed: number) {
  rect(ctx, x, 0, TILE, TILE, C.mortar);
  const rows = [0, 8, 16, 24];
  rows.forEach((ry, r) => {
    let cx = r % 2 ? -5 : 0;
    let i = 0;
    while (cx < TILE) {
      const w = 7 + Math.floor(hash(seed + r, i) * 5);
      const sx = Math.max(cx + 1, 0);
      const ex = Math.min(cx + w - 1, TILE);
      if (ex - sx > 1) {
        const tone = hash(seed, r * 10 + i) < 0.25 ? C.stoneShade : C.stone;
        rect(ctx, x + sx, ry + 1, ex - sx, 6, tone);
        rect(ctx, x + sx, ry + 1, ex - sx, 1, C.stoneLight);
        rect(ctx, x + sx, ry + 6, ex - sx, 1, C.stoneDark);
        rect(ctx, x + ex - 1, ry + 1, 1, 6, C.stoneDark);
      }
      cx += w;
      i++;
    }
  });
}

function grassBase(ctx: Ctx, x: number, seed: number) {
  rect(ctx, x, 0, TILE, TILE, C.grass);
  specks(ctx, x, 0, C.grassLight, 12, seed);
  specks(ctx, x, 0, C.grassDark, 10, seed + 1);
  // a few little tufts
  for (let i = 0; i < 4; i++) {
    const tx = x + 3 + Math.floor(hash(seed + 2, i) * 24);
    const ty = 4 + Math.floor(hash(seed + 3, i) * 24);
    rect(ctx, tx, ty, 1, 2, C.grassDark);
    rect(ctx, tx + 2, ty, 1, 2, C.grassDark);
    rect(ctx, tx + 1, ty - 1, 1, 3, C.grassLight);
  }
}

/** Dense blades; `onlyTips` draws just the front blades for the layer above the player. */
function tallGrass(ctx: Ctx, x: number, onlyTips: boolean) {
  if (!onlyTips) {
    rect(ctx, x, 0, TILE, TILE, C.tall);
    specks(ctx, x, 0, C.tallDark, 18, 41, 2);
  }
  const rows = onlyTips ? [20] : [4, 12, 20];
  for (const by of rows) {
    for (let i = 0; i < 8; i++) {
      const bx = x + i * 4 + (by % 8 === 4 ? 2 : 0);
      const h = 9 + Math.floor(hash(by, i) * 4);
      rect(ctx, bx, by + 12 - h, 2, h, C.tallDark);
      rect(ctx, bx + 1, by + 12 - h - 2, 1, 4, C.tall);
      rect(ctx, bx, by + 12 - h - 1, 1, 2, C.tallLight);
    }
  }
  if (!onlyTips) {
    // a torn receipt caught in the grass: something lives in here
    rect(ctx, x + 21, 6, 5, 4, C.paper);
    rect(ctx, x + 22, 7, 3, 1, C.crimson);
  }
}

function waterBase(ctx: Ctx, x: number, phase: number) {
  rect(ctx, x, 0, TILE, TILE, C.water);
  for (let i = 0; i < 5; i++) {
    const wy = 3 + i * 6;
    const wx = ((i * 11 + phase * 4) % 28);
    rect(ctx, x + wx, wy, 7, 1, C.waterLight);
    rect(ctx, x + ((wx + 15) % 26), wy + 3, 4, 1, C.waterDark);
  }
}

function stall(ctx: Ctx, x: number, side: "L" | "R", goodsSeed: number) {
  // ground (cobbles under the stall)
  cobbles(ctx, x, 17 + goodsSeed);
  // counter body
  rect(ctx, x, 15, TILE, 17, C.wood);
  rect(ctx, x, 15, TILE, 2, C.woodLight);
  rect(ctx, x, 23, TILE, 1, C.woodDark);
  rect(ctx, x, 30, TILE, 2, C.woodDark);
  for (let px = 4; px < TILE; px += 8) rect(ctx, x + px, 24, 1, 6, C.woodDark);
  // cloth draped on the counter front: a paper talisman with a red diamond
  const cx = side === "L" ? 9 : 14;
  rect(ctx, x + cx, 24, 9, 7, C.paper);
  rect(ctx, x + cx + 3, 25, 3, 1, C.crimson);
  rect(ctx, x + cx + 2, 26, 5, 2, C.crimson);
  rect(ctx, x + cx + 3, 28, 3, 1, C.crimson);
  // goods on the counter top
  if (side === "L") {
    rect(ctx, x + 6, 16, 6, 3, C.gold); rect(ctx, x + 6, 16, 6, 1, "#F6D77A"); rect(ctx, x + 8, 17, 2, 1, C.goldDark);
    rect(ctx, x + 15, 16, 8, 3, C.paper); rect(ctx, x + 15, 18, 8, 1, C.stoneShade);
    rect(ctx, x + 25, 15, 4, 4, C.crimson);
  } else {
    rect(ctx, x + 3, 16, 9, 3, C.paper); rect(ctx, x + 3, 17, 9, 1, C.crimson);
    rect(ctx, x + 15, 15, 5, 4, C.gold); rect(ctx, x + 16, 16, 3, 1, C.goldDark);
    rect(ctx, x + 22, 16, 6, 3, C.woodLight); rect(ctx, x + 22, 16, 6, 1, C.paper);
  }
  // posts
  const postX = side === "L" ? 1 : TILE - 4;
  rect(ctx, x + postX, 4, 3, 28, C.woodDark);
  // striped awning (top-down 3/4: we see its slope and the scalloped front edge)
  for (let i = 0; i < 4; i++) rect(ctx, x + i * 8, 0, 8, 11, i % 2 ? C.paper : C.crimson);
  rect(ctx, x, 0, TILE, 1, C.crimsonDark);
  rect(ctx, x, 10, TILE, 1, "rgba(26,23,38,0.25)");
  for (let i = 0; i < 8; i++) {
    const color = Math.floor(i / 2) % 2 ? C.paper : C.crimson;
    rect(ctx, x + i * 4, 11, 4, 2, color);
    rect(ctx, x + i * 4 + 1, 13, 2, 1, color);
  }
  // shadow under the awning
  rect(ctx, x, 14, TILE, 1, "rgba(26,23,38,0.35)");
}

function lanternPost(ctx: Ctx, x: number) {
  cobbles(ctx, x, 61);
  rect(ctx, x + 10, 28, 13, 3, "rgba(26,23,38,0.25)"); // shadow
  rect(ctx, x + 15, 12, 3, 18, C.woodDark);
  rect(ctx, x + 12, 28, 9, 3, C.woodDark);
  // hanging arm and lantern
  rect(ctx, x + 12, 2, 9, 2, C.woodDark);
  rect(ctx, x + 11, 4, 11, 2, C.gold);
  rect(ctx, x + 10, 6, 13, 9, "#E2672E");
  rect(ctx, x + 12, 7, 9, 7, "#F7C35C");
  rect(ctx, x + 14, 8, 5, 5, "#FFF1C4");
  rect(ctx, x + 10, 10, 13, 1, "#C8502A");
  rect(ctx, x + 11, 15, 11, 2, C.gold);
  rect(ctx, x + 16, 17, 1, 3, C.crimson);
  rect(ctx, x + 15, 19, 3, 2, C.crimson);
}

function board(ctx: Ctx, x: number) {
  rect(ctx, x, 0, TILE, TILE, C.stone);
  plazaPattern(ctx, x);
  rect(ctx, x + 3, 29, 26, 3, "rgba(26,23,38,0.3)");
  // legs and little roof
  rect(ctx, x + 5, 20, 3, 11, C.woodDark);
  rect(ctx, x + 24, 20, 3, 11, C.woodDark);
  rect(ctx, x + 1, 0, 30, 3, C.crimsonDark);
  rect(ctx, x + 2, 3, 28, 2, C.woodDark);
  // board face
  rect(ctx, x + 3, 5, 26, 17, C.wood);
  rect(ctx, x + 3, 5, 26, 1, C.woodLight);
  // pinned rules, each with a red wax seal
  rect(ctx, x + 5, 7, 8, 9, C.paper); rect(ctx, x + 6, 9, 6, 1, C.stoneDark); rect(ctx, x + 6, 11, 5, 1, C.stoneDark);
  rect(ctx, x + 8, 13, 3, 3, C.crimson);
  rect(ctx, x + 15, 7, 7, 7, C.paper); rect(ctx, x + 16, 9, 5, 1, C.stoneDark);
  rect(ctx, x + 17, 11, 3, 3, C.crimson);
  rect(ctx, x + 23, 8, 5, 12, C.paper); rect(ctx, x + 24, 10, 3, 1, C.stoneDark); rect(ctx, x + 24, 12, 3, 1, C.stoneDark);
  rect(ctx, x + 15, 15, 7, 5, C.paper); rect(ctx, x + 16, 17, 4, 1, C.stoneDark);
  rect(ctx, x + 24, 16, 3, 3, C.gold);
}

function pier(ctx: Ctx, x: number) {
  rect(ctx, x, 0, TILE, TILE, C.woodLight);
  for (let i = 0; i < 4; i++) {
    rect(ctx, x + i * 8, 0, 1, TILE, C.woodDark);
    rect(ctx, x + i * 8 + 1, 0, 1, TILE, "#BC8650");
  }
  rect(ctx, x + 3, 6, 1, 1, C.woodDark); rect(ctx, x + 11, 22, 1, 1, C.woodDark);
  rect(ctx, x + 19, 12, 1, 1, C.woodDark); rect(ctx, x + 27, 28, 1, 1, C.woodDark);
}

/** Cream mosaic floor with crimson and teal diamonds, like the plaza in the concept art. */
function plazaPattern(ctx: Ctx, x: number) {
  for (let qy = 0; qy < 2; qy++) {
    for (let qx = 0; qx < 2; qx++) {
      const ox = x + qx * 16, oy = qy * 16;
      rect(ctx, ox, oy, 16, 16, C.stone);
      rect(ctx, ox, oy, 16, 1, C.stoneShade);
      rect(ctx, ox, oy, 1, 16, C.stoneShade);
      const color = (qx + qy) % 2 ? C.teal : C.crimson;
      // diamond outline
      for (let d = 0; d < 5; d++) {
        rect(ctx, ox + 8 - d, oy + 3 + d, 1, 1, color);
        rect(ctx, ox + 8 + d, oy + 3 + d, 1, 1, color);
        rect(ctx, ox + 8 - d, oy + 13 - d, 1, 1, color);
        rect(ctx, ox + 8 + d, oy + 13 - d, 1, 1, color);
      }
      rect(ctx, ox + 8, oy + 8, 1, 1, C.gold);
    }
  }
}

function crate(ctx: Ctx, x: number) {
  grassBase(ctx, x, 91);
  rect(ctx, x + 3, 27, 27, 4, "rgba(26,23,38,0.3)");
  rect(ctx, x + 3, 8, 26, 21, C.wood);
  rect(ctx, x + 3, 8, 26, 2, C.woodLight);
  rect(ctx, x + 3, 18, 26, 1, C.woodDark);
  rect(ctx, x + 3, 28, 26, 1, C.woodDark);
  rect(ctx, x + 3, 8, 2, 21, C.woodDark);
  rect(ctx, x + 27, 8, 2, 21, C.woodDark);
  // ledgers poking out of the top
  const spines = [C.crimson, C.sorrow, C.paper, C.goldDark, C.crimsonDark, C.paper];
  spines.forEach((c, i) => rect(ctx, x + 6 + i * 4, 3 + (i % 2) * 2, 3, 7, c));
  rect(ctx, x + 6, 21, 20, 5, C.paper);
  rect(ctx, x + 13, 22, 6, 3, C.crimson);
}

function plant(ctx: Ctx, x: number) {
  rect(ctx, x, 0, TILE, TILE, C.stone);
  plazaPattern(ctx, x);
  rect(ctx, x + 7, 28, 19, 3, "rgba(26,23,38,0.3)");
  // glazed pot
  rect(ctx, x + 9, 18, 15, 11, C.pot);
  rect(ctx, x + 8, 17, 17, 3, C.potLight);
  rect(ctx, x + 11, 22, 11, 2, C.paper);
  rect(ctx, x + 15, 22, 3, 2, C.crimson);
  // leaves
  const leaves: [number, number, number, number][] = [[6, 6, 6, 4], [20, 5, 6, 4], [11, 2, 4, 9], [17, 1, 4, 10], [8, 11, 7, 4], [18, 10, 8, 4], [14, 7, 5, 9]];
  for (const [lx, ly, lw, lh] of leaves) {
    rect(ctx, x + lx, ly, lw, lh, C.leaf);
    rect(ctx, x + lx, ly, lw - 1, 1, C.leafLight);
  }
}

function quay(ctx: Ctx, x: number) {
  waterBase(ctx, x, 0);
  rect(ctx, x, 0, TILE, 9, C.stone);
  rect(ctx, x, 0, TILE, 2, C.stoneLight);
  for (let i = 0; i < 4; i++) rect(ctx, x + i * 8 + 7, 2, 1, 7, C.stoneDark);
  rect(ctx, x, 9, TILE, 3, C.stoneDark);
  rect(ctx, x, 12, TILE, 2, "rgba(26,23,38,0.35)");
}

function rug(ctx: Ctx, x: number) {
  rect(ctx, x, 0, TILE, TILE, C.crimson);
  rect(ctx, x, 2, TILE, 1, C.gold);
  rect(ctx, x, 29, TILE, 1, C.gold);
  for (let d = 0; d < 7; d++) {
    rect(ctx, x + 16 - d, 9 + d, 1, 1, C.paper);
    rect(ctx, x + 16 + d, 9 + d, 1, 1, C.paper);
    rect(ctx, x + 16 - d, 23 - d, 1, 1, C.paper);
    rect(ctx, x + 16 + d, 23 - d, 1, 1, C.paper);
  }
  rect(ctx, x + 15, 15, 3, 3, C.gold);
  specks(ctx, x, 0, C.crimsonDark, 10, 5);
}

function bannerPost(ctx: Ctx, x: number) {
  rect(ctx, x, 0, TILE, TILE, C.stone);
  plazaPattern(ctx, x);
  rect(ctx, x + 9, 29, 15, 3, "rgba(26,23,38,0.3)");
  rect(ctx, x + 15, 0, 3, 31, C.woodDark);
  rect(ctx, x + 14, 0, 5, 2, C.gold);
  rect(ctx, x + 8, 3, 17, 2, C.woodDark);
  // hanging banner: crimson with a cream diamond and tassels
  rect(ctx, x + 9, 5, 15, 17, C.crimson);
  rect(ctx, x + 9, 5, 15, 1, C.crimsonDark);
  for (let d = 0; d < 4; d++) {
    rect(ctx, x + 16 - d, 9 + d, 1, 1, C.paper); rect(ctx, x + 16 + d, 9 + d, 1, 1, C.paper);
    rect(ctx, x + 16 - d, 16 - d, 1, 1, C.paper); rect(ctx, x + 16 + d, 16 - d, 1, 1, C.paper);
  }
  rect(ctx, x + 9, 22, 3, 3, C.crimson); rect(ctx, x + 15, 22, 3, 4, C.crimson); rect(ctx, x + 21, 22, 3, 3, C.crimson);
}

function wall(ctx: Ctx, x: number, banner: boolean) {
  // top cap (we look down on it), then the face
  rect(ctx, x, 0, TILE, 11, C.stoneLight);
  rect(ctx, x, 9, TILE, 2, C.stoneShade);
  rect(ctx, x, 11, TILE, 21, C.stone);
  for (let r = 0; r < 3; r++) {
    const y = 11 + r * 7;
    rect(ctx, x, y + 6, TILE, 1, C.stoneDark);
    const off = r % 2 ? 4 : 12;
    rect(ctx, x + off, y, 1, 6, C.stoneDark);
    rect(ctx, x + off + 16, y, 1, 6, C.stoneDark);
  }
  rect(ctx, x, 30, TILE, 2, "rgba(26,23,38,0.35)");
  if (banner) {
    rect(ctx, x + 8, 10, 16, 2, C.woodDark);
    rect(ctx, x + 9, 12, 14, 16, C.crimson);
    for (let d = 0; d < 4; d++) {
      rect(ctx, x + 16 - d, 15 + d, 1, 1, C.gold); rect(ctx, x + 16 + d, 15 + d, 1, 1, C.gold);
      rect(ctx, x + 16 - d, 22 - d, 1, 1, C.gold); rect(ctx, x + 16 + d, 22 - d, 1, 1, C.gold);
    }
    rect(ctx, x + 9, 28, 2, 3, C.crimson); rect(ctx, x + 21, 28, 2, 3, C.crimson);
  }
}

/** A 2x2 plaza fountain; `qx`/`qy` choose the quarter. */
function fountain(ctx: Ctx, x: number, qx: number, qy: number) {
  rect(ctx, x, 0, TILE, TILE, C.stone);
  plazaPattern(ctx, x);
  const cx = x + (qx === 0 ? 32 : 0); // fountain centre, in this tile's coordinates
  const cy = qy === 0 ? 34 : 2;
  ctx.save();
  ctx.beginPath();
  ctx.rect(x, 0, TILE, TILE);
  ctx.clip();
  const disc = (r: number, color: string, dy = 0) => {
    ctx.fillStyle = color;
    for (let yy = -r; yy <= r; yy++) {
      const half = Math.floor(Math.sqrt(r * r - yy * yy) * 1.15);
      ctx.fillRect(cx - half, Math.round(cy + yy * 0.8 + dy), half * 2, 1);
    }
  };
  disc(27, "rgba(26,23,38,0.25)", 3);
  disc(26, C.stoneDark);
  disc(24, C.stoneLight);
  disc(21, C.waterDark);
  disc(19, C.water);
  // paper slips floating in the basin
  rect(ctx, cx - 12, cy - 8, 4, 3, C.paper);
  rect(ctx, cx + 8, cy + 6, 4, 3, C.paper);
  rect(ctx, cx + 9, cy + 7, 2, 1, C.crimson);
  disc(7, C.stoneDark, -2);
  disc(5, C.stoneLight, -3);
  disc(3, C.waterLight, -4);
  ctx.restore();
}

export function drawTiles(ctx: Ctx) {
  const at = (i: number) => i * TILE;
  cobbles(ctx, at(T.path), 3);
  grassBase(ctx, at(T.grass), 7);
  tallGrass(ctx, at(T.tallGrass), false);
  waterBase(ctx, at(T.water), 0);
  wall(ctx, at(T.wall), false);
  stall(ctx, at(T.stall), "L", 1);
  lanternPost(ctx, at(T.lantern));
  board(ctx, at(T.board));
  pier(ctx, at(T.pier));
  stall(ctx, at(T.stallR), "R", 2);
  plazaPattern(ctx, at(T.plaza));
  cobbles(ctx, at(T.path2), 29);
  grassBase(ctx, at(T.flowers), 13);
  {
    const x = at(T.flowers);
    const blooms: [number, number, string][] = [[6, 8, C.rage], [20, 5, C.paper], [13, 20, C.gold], [25, 22, C.rage], [4, 25, C.paper]];
    for (const [fx, fy, color] of blooms) {
      rect(ctx, x + fx, fy, 3, 3, color);
      rect(ctx, x + fx + 1, fy + 1, 1, 1, C.gold);
    }
  }
  crate(ctx, at(T.crate));
  plant(ctx, at(T.plant));
  quay(ctx, at(T.quay));
  waterBase(ctx, at(T.water2), 3);
  rug(ctx, at(T.rug));
  tallGrass(ctx, at(T.tallGrassOver), true);
  bannerPost(ctx, at(T.bannerPost));
  wall(ctx, at(T.wallBanner), true);
  fountain(ctx, at(T.fountainTL), 0, 0);
  fountain(ctx, at(T.fountainTR), 1, 0);
  fountain(ctx, at(T.fountainBL), 0, 1);
  fountain(ctx, at(T.fountainBR), 1, 1);
}

/** Lantern Ward test map: a market street, a plaza with the Rite board, tall grass, and the harbor. */
export function buildLanternWard(cols = 44, rows = 30): number[][] {
  const m: number[][] = Array.from({ length: rows }, () => Array<number>(cols).fill(T.grass));
  const fill = (x0: number, y0: number, w: number, h: number, t: number) => {
    for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) if (m[y] && x >= 0 && x < cols) m[y]![x] = t;
  };
  const put = (x: number, y: number, t: number) => { if (m[y] && x >= 0 && x < cols) m[y]![x] = t; };

  // scattered flowers on the grass
  for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) if (hash(x, y + 99) < 0.07) m[y]![x] = T.flowers;

  // harbor at the bottom: a stone quay, open water, and a pier going out
  fill(0, rows - 6, cols, 6, T.water);
  fill(0, rows - 6, cols, 1, T.quay);
  fill(20, rows - 6, 3, 5, T.pier);

  // main street and cross street (two cobble patterns, mixed)
  const road = (x0: number, y0: number, w: number, h: number) => {
    for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) put(x, y, hash(x, y) < 0.5 ? T.path : T.path2);
  };
  road(1, 12, cols - 2, 3);
  road(20, 1, 3, rows - 7);

  // plaza with the Rite board, a rug, a fountain, banners and potted plants
  fill(16, 9, 11, 9, T.plaza);
  put(21, 10, T.board);
  fill(20, 11, 3, 1, T.rug);
  put(17, 15, T.fountainTL); put(18, 15, T.fountainTR);
  put(17, 16, T.fountainBL); put(18, 16, T.fountainBR);
  put(16, 9, T.plant); put(26, 9, T.plant); put(16, 17, T.plant); put(26, 17, T.plant);
  put(18, 9, T.bannerPost); put(24, 9, T.bannerPost);
  put(25, 16, T.crate);

  // market stalls along the street (two tiles each), lanterns between them on both sides
  for (let x = 3; x < cols - 3; x += 4) {
    if (x >= 14 && x <= 27) continue;
    put(x, 11, T.stall); put(x + 1, 11, T.stallR);
    put(x, 15, T.stall); put(x + 1, 15, T.stallR);
    put(x + 2, 11, T.lantern);
    put(x + 2, 15, T.lantern);
    if (hash(x, 3) < 0.6) put(x + 1, 10, T.crate);
  }

  // tall grass where curses lurk
  fill(3, 3, 9, 6, T.tallGrass);
  fill(31, 3, 9, 6, T.tallGrass);
  fill(4, 18, 10, 4, T.tallGrass);
  fill(30, 18, 10, 4, T.tallGrass);

  // town wall with crimson banners
  fill(0, 0, cols, 1, T.wall);
  for (let x = 3; x < cols; x += 6) put(x, 0, T.wallBanner);
  fill(0, 0, 1, rows - 6, T.wall);
  fill(cols - 1, 0, 1, rows - 6, T.wall);
  return m;
}

/** The layer drawn above the player: blade tips over tall grass, so feet sink into it. */
export function buildOverLayer(base: number[][]): number[][] {
  return base.map((row) => row.map((t) => (t === T.tallGrass ? T.tallGrassOver : -1)));
}

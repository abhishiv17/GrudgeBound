/**
 * Placeholder tiles, drawn in code until the real 32x32 tileset arrives (docs/ART_PROMPTS.md, prompt D).
 * Colors follow the style guide palette.
 */
export const TILE = 32;

export const T = {
  path: 0,
  grass: 1,
  tallGrass: 2,
  water: 3,
  wall: 4,
  stall: 5,
  lantern: 6,
  board: 7,
  pier: 8,
} as const;

export const SOLID = [T.water, T.wall, T.stall, T.lantern, T.board];
export const TILE_COUNT = 9;

type Ctx = CanvasRenderingContext2D;

function specks(ctx: Ctx, x: number, color: string, count: number, seed: number) {
  ctx.fillStyle = color;
  let s = seed;
  for (let i = 0; i < count; i++) {
    s = (s * 9301 + 49297) % 233280;
    const px = s % TILE;
    s = (s * 9301 + 49297) % 233280;
    const py = s % TILE;
    ctx.fillRect(x + px, py, 2, 2);
  }
}

export function drawTiles(ctx: Ctx) {
  const at = (i: number) => i * TILE;

  // path: cream paving stones
  let x = at(T.path);
  ctx.fillStyle = "#E8DCC0"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#D6C7A6"; ctx.fillRect(x, 15, TILE, 2); ctx.fillRect(x + 15, 0, 2, 15); ctx.fillRect(x + 5, 17, 2, 15);
  specks(ctx, x, "#CBB993", 6, 3);

  // grass
  x = at(T.grass);
  ctx.fillStyle = "#6E9E4F"; ctx.fillRect(x, 0, TILE, TILE);
  specks(ctx, x, "#86B563", 10, 7);
  specks(ctx, x, "#5A8840", 6, 11);

  // tall grass (curses lurk here)
  x = at(T.tallGrass);
  ctx.fillStyle = "#4F7F3A"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#3C6A2C";
  for (let i = 0; i < 6; i++) { const bx = x + 2 + i * 5; ctx.fillRect(bx, 8, 2, 20); ctx.fillRect(bx + 2, 4, 2, 10); }
  ctx.fillStyle = "#7DAA5A";
  for (let i = 0; i < 6; i++) ctx.fillRect(x + 4 + i * 5, 6, 1, 8);

  // water
  x = at(T.water);
  ctx.fillStyle = "#2F7FA8"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#5FB0D3"; ctx.fillRect(x + 4, 8, 10, 2); ctx.fillRect(x + 18, 20, 10, 2);

  // wall: dark stone bricks
  x = at(T.wall);
  ctx.fillStyle = "#4A4458"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#3A3547";
  for (let r = 0; r < 4; r++) { ctx.fillRect(x, r * 8 + 7, TILE, 1); ctx.fillRect(x + (r % 2 ? 4 : 18), r * 8, 1, 8); }

  // market stall: wood + crimson striped awning
  x = at(T.stall);
  ctx.fillStyle = "#8A5A35"; ctx.fillRect(x, 12, TILE, 20);
  ctx.fillStyle = "#6E4526"; ctx.fillRect(x, 20, TILE, 2);
  for (let i = 0; i < 4; i++) { ctx.fillStyle = i % 2 ? "#F3E9D2" : "#B0153A"; ctx.fillRect(x + i * 8, 0, 8, 12); }
  ctx.fillStyle = "#D9A532"; ctx.fillRect(x + 6, 15, 4, 4); ctx.fillRect(x + 20, 15, 4, 4);

  // lantern post
  x = at(T.lantern);
  ctx.fillStyle = "#E8DCC0"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#3A2A20"; ctx.fillRect(x + 15, 10, 3, 22);
  ctx.fillStyle = "#E2672E"; ctx.fillRect(x + 10, 2, 13, 12);
  ctx.fillStyle = "#F7C35C"; ctx.fillRect(x + 13, 5, 7, 6);
  ctx.fillStyle = "#B0153A"; ctx.fillRect(x + 15, 14, 3, 3);

  // Rite notice board
  x = at(T.board);
  ctx.fillStyle = "#E8DCC0"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#6E4526"; ctx.fillRect(x + 2, 2, 28, 20); ctx.fillRect(x + 6, 22, 3, 10); ctx.fillRect(x + 23, 22, 3, 10);
  ctx.fillStyle = "#F3E9D2"; ctx.fillRect(x + 5, 5, 9, 7); ctx.fillRect(x + 17, 6, 9, 9); ctx.fillRect(x + 7, 14, 8, 6);
  ctx.fillStyle = "#B0153A"; ctx.fillRect(x + 20, 9, 3, 3);

  // pier planks over water
  x = at(T.pier);
  ctx.fillStyle = "#9C6B42"; ctx.fillRect(x, 0, TILE, TILE);
  ctx.fillStyle = "#7A4F2E"; for (let i = 0; i < 4; i++) ctx.fillRect(x, i * 8 + 7, TILE, 1);
}

/** Lantern Ward test map: a market street, a plaza with the Rite board, tall grass, and the harbor. */
export function buildLanternWard(cols = 44, rows = 30): number[][] {
  const m: number[][] = Array.from({ length: rows }, () => Array<number>(cols).fill(T.grass));
  const rect = (x0: number, y0: number, w: number, h: number, t: number) => {
    for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) if (m[y] && x < cols) m[y]![x] = t;
  };
  // harbor at the bottom, pier going out
  rect(0, rows - 6, cols, 6, T.water);
  rect(20, rows - 6, 3, 5, T.pier);
  // main street and cross street
  rect(1, 12, cols - 2, 3, T.path);
  rect(20, 1, 3, rows - 7, T.path);
  // plaza with the Rite board
  rect(16, 9, 11, 9, T.path);
  m[10]![21] = T.board;
  // market stalls along the street, lanterns between
  for (let x = 3; x < cols - 3; x += 4) {
    if (x >= 15 && x <= 27) continue;
    m[11]![x] = T.stall; m[11]![x + 1] = T.stall;
    m[15]![x] = T.stall; m[15]![x + 1] = T.stall;
    m[11]![x + 2] = T.lantern;
  }
  // tall grass where curses lurk
  rect(3, 3, 9, 6, T.tallGrass);
  rect(31, 3, 9, 6, T.tallGrass);
  rect(4, 18, 10, 4, T.tallGrass);
  rect(30, 18, 10, 4, T.tallGrass);
  // town wall
  rect(0, 0, cols, 1, T.wall);
  rect(0, 0, 1, rows - 6, T.wall);
  rect(cols - 1, 0, 1, rows - 6, T.wall);
  return m;
}

/**
 * The player: a young exorcist with dark hair tied up, a long red scarf, a cream jacket,
 * indigo trousers and a paper talisman in hand. Deliberately no green tunic, no pointed cap
 * and no satchel (docs/ART_PROMPTS.md: must not read as a famous hero).
 *
 * Pure data (no Phaser), so React can draw the same character on the title screen.
 */
import { INK, outlineGrid, type Grid } from "./pixelArt";

export const PLAYER_FRAME = 32;
/** Rows of the player sheet. Left-facing frames are the side row flipped. */
export const PLAYER_ROW = { down: 0, up: 1, side: 2 } as const;
export const PLAYER_FRAMES_PER_ROW = 3; // 0 idle, 1 step A, 2 step B

const PC = {
  hair: "#2A2238", hairHi: "#4A3F63", tie: "#B0153A",
  skin: "#F2C9A0", skinSh: "#D9A27E", blush: "#E8A08A", eye: INK,
  scarf: "#C8302A", scarfDk: "#8E1A2A",
  top: "#EFE6D2", topSh: "#C9BFA8", belt: "#D9A532",
  pants: "#2B3A66", pantsSh: "#1E2A4D", boot: "#4A2E1E",
  paper: "#F3E9D2", mark: "#B0153A",
};

function newGrid(w: number, h: number): Grid {
  return Array.from({ length: h }, () => Array<string | null>(w).fill(null));
}

function painter(g: Grid) {
  return (x: number, y: number, w: number, h: number, c: string) => {
    for (let yy = y; yy < y + h; yy++) for (let xx = x; xx < x + w; xx++) if (g[yy] && xx >= 0 && xx < g[yy]!.length) g[yy]![xx] = c;
  };
}

function playerDown(f: number): Grid {
  const g = newGrid(32, 32);
  const R = painter(g);
  const swing = f === 1 ? 1 : f === 2 ? -1 : 0;
  // talisman held in the right hand (drawn first so the hand overlaps it)
  R(23, 19 - swing, 3, 6, PC.paper); R(24, 21 - swing, 1, 2, PC.mark);
  // head
  R(10, 5, 12, 9, PC.hair); R(11, 3, 10, 2, PC.hair); R(14, 1, 4, 2, PC.hair); R(14, 3, 4, 1, PC.tie);
  R(13, 4, 4, 1, PC.hairHi);
  R(12, 8, 8, 6, PC.skin);
  R(12, 8, 2, 1, PC.hair); R(15, 8, 2, 1, PC.hair); R(18, 8, 2, 1, PC.hair);
  R(13, 10, 2, 2, PC.eye); R(17, 10, 2, 2, PC.eye);
  R(12, 12, 1, 1, PC.blush); R(19, 12, 1, 1, PC.blush);
  R(13, 13, 6, 1, PC.skinSh);
  // arms and body
  R(8, 17, 2, 5 + swing, PC.top); R(8, 22 + swing, 2, 1, PC.skin);
  R(22, 17, 2, 5 - swing, PC.topSh); R(22, 22 - swing, 2, 1, PC.skin);
  R(10, 17, 12, 6, PC.top); R(19, 17, 3, 6, PC.topSh); R(15, 17, 2, 2, PC.topSh);
  R(10, 23, 12, 1, PC.belt);
  // scarf with the tail hanging over the right shoulder
  R(9, 14, 14, 3, PC.scarf); R(9, 16, 14, 1, PC.scarfDk);
  R(19, 15, 3, 3, PC.scarf);
  R(20, 17, 3, 4 + (f === 1 ? 1 : 0), PC.scarf); R(22, 18, 1, 3, PC.scarfDk);
  // legs and boots
  R(11, 24, 10, 3, PC.pants); R(19, 24, 2, 3, PC.pantsSh);
  const l = f === 1 ? -1 : 0, r = f === 2 ? -1 : 0;
  R(11, 27, 4, 2 + l, PC.pants); R(11, 29 + l, 4, 2, PC.boot);
  R(17, 27, 4, 2 + r, PC.pantsSh); R(17, 29 + r, 4, 2, PC.boot);
  return g;
}

function playerUp(f: number): Grid {
  const g = newGrid(32, 32);
  const R = painter(g);
  const swing = f === 1 ? 1 : f === 2 ? -1 : 0;
  R(8, 17, 2, 5 - swing, PC.topSh); R(8, 22 - swing, 2, 1, PC.skin);
  R(22, 17, 2, 5 + swing, PC.topSh); R(22, 22 + swing, 2, 1, PC.skin);
  R(10, 17, 12, 6, PC.top); R(10, 17, 3, 6, PC.topSh); R(19, 17, 3, 6, PC.topSh);
  R(10, 23, 12, 1, PC.belt);
  R(9, 14, 14, 3, PC.scarf); R(9, 16, 14, 1, PC.scarfDk);
  R(10, 4, 12, 10, PC.hair); R(11, 3, 10, 1, PC.hair); R(14, 1, 4, 2, PC.hair);
  R(12, 5, 3, 2, PC.hairHi); R(17, 6, 2, 1, PC.hairHi);
  R(10, 11, 1, 2, PC.skin); R(21, 11, 1, 2, PC.skin); // ears
  // ponytail down the back, red tie
  R(14, 12, 4, 1, PC.tie);
  R(14, 13, 4, 5 + (f === 0 ? 0 : 1), PC.hair); R(15, 18 + (f === 0 ? 0 : 1), 2, 1, PC.hair);
  R(15, 14, 1, 3, PC.hairHi);
  R(11, 24, 10, 3, PC.pants); R(11, 24, 2, 3, PC.pantsSh);
  const l = f === 2 ? -1 : 0, r = f === 1 ? -1 : 0;
  R(11, 27, 4, 2 + l, PC.pants); R(11, 29 + l, 4, 2, PC.boot);
  R(17, 27, 4, 2 + r, PC.pants); R(17, 29 + r, 4, 2, PC.boot);
  return g;
}

function playerSide(f: number): Grid {
  const g = newGrid(32, 32);
  const R = painter(g);
  const wave = f === 1 ? 1 : f === 2 ? -1 : 0;
  // scarf tail and ponytail streaming behind (left)
  R(5, 14 + wave, 6, 2, PC.scarf); R(3, 15 + wave, 3, 2, PC.scarfDk);
  R(6, 6 - wave, 4, 3, PC.hair); R(4, 7 - wave, 3, 3, PC.hair); R(7, 7 - wave, 2, 1, PC.hairHi);
  R(10, 6, 2, 2, PC.tie);
  // back leg
  if (f === 1) { R(11, 24, 4, 4, PC.pantsSh); R(10, 28, 4, 2, PC.boot); }
  if (f === 2) { R(13, 24, 4, 5, PC.pantsSh); R(13, 29, 5, 2, PC.boot); }
  // body
  R(12, 17, 8, 6, PC.top); R(12, 17, 2, 6, PC.topSh);
  R(12, 23, 8, 1, PC.belt);
  // head
  R(11, 4, 10, 10, PC.hair); R(12, 3, 8, 1, PC.hair);
  R(16, 7, 5, 6, PC.skin); R(15, 10, 1, 3, PC.skin);
  R(16, 7, 5, 1, PC.hair); R(20, 8, 1, 1, PC.hair); R(13, 5, 4, 1, PC.hairHi);
  R(18, 9, 2, 2, PC.eye); R(19, 12, 1, 1, PC.blush); R(16, 12, 4, 1, PC.skinSh);
  // scarf wrap
  R(11, 14, 10, 3, PC.scarf); R(11, 16, 10, 1, PC.scarfDk);
  // front leg
  if (f === 0) { R(13, 24, 6, 5, PC.pants); R(13, 29, 7, 2, PC.boot); }
  if (f === 1) { R(16, 24, 4, 5, PC.pants); R(17, 29, 4, 2, PC.boot); }
  if (f === 2) { R(15, 24, 4, 5, PC.pants); R(15, 29, 5, 2, PC.boot); }
  // front arm with the talisman
  const ax = f === 1 ? 17 : f === 2 ? 13 : 15;
  R(ax, 17, 3, 5, PC.topSh); R(ax, 22, 3, 1, PC.skin);
  R(ax + 2, 19, 2, 5, PC.paper); R(ax + 2, 21, 1, 1, PC.mark);
  return g;
}

const DRAW = [playerDown, playerUp, playerSide];

/** One outlined 32x32 frame of the player. */
export function playerFrame(row: keyof typeof PLAYER_ROW, frame: number): Grid {
  return outlineGrid(DRAW[PLAYER_ROW[row]]!(frame));
}

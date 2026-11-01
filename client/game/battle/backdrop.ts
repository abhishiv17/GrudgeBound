/**
 * Battle backdrop: a Lantern Ward night-market square at dusk, painted once into a 320x180
 * pixel canvas and shown at 3x. Chalk-and-ink binding circles mark where each curse stands.
 */
import type * as Phaser from "phaser";
import { Pixels, C, makePixelTexture, noise } from "./pixels";

export const BACKDROP_W = 320;
export const BACKDROP_H = 180;
export const BACKDROP_SCALE = 3;
export const HORIZON = 62;
/** Feet positions in backdrop pixels (multiply by BACKDROP_SCALE for screen coordinates). */
export const WILD_SPOT = { x: 236, y: 82, rx: 44, ry: 9 };
export const PLAYER_SPOT = { x: 78, y: 121, rx: 54, ry: 11 };

const SKY = ["#231B38", "#2E2245", "#3F2A50", "#583455", "#7A4152", "#A2534C", "#C9714A", "#E3955A"];
const ROOF = "#2A2238";
const ROOF_EDGE = "#3B3050";
const STONE = "#E6D6B0";
const STONE_ALT = "#DDCBA0";
const GROUT = "#B39C70";

function ellipseRing(p: Pixels, cx: number, cy: number, rx: number, ry: number, t: number, color: string, every = 0) {
  for (let y = Math.floor(cy - ry - 1); y <= cy + ry + 1; y++) for (let x = Math.floor(cx - rx - 1); x <= cx + rx + 1; x++) {
    const d = Math.hypot((x - cx) / rx, (y - cy) / ry);
    const inner = 1 - t / Math.min(rx, ry * 2);
    if (d <= 1 && d > inner) {
      if (every && Math.floor((Math.atan2(y - cy, x - cx) + Math.PI) * 6) % every === 0) continue;
      p.px(x, y, color);
    }
  }
}

function sky(p: Pixels) {
  const band = HORIZON / SKY.length;
  for (let y = 0; y < HORIZON; y++) {
    const i = Math.min(SKY.length - 1, Math.floor(y / band));
    const f = y / band - i;
    for (let x = 0; x < BACKDROP_W; x++) {
      // ordered dither into the next band near the boundary
      const next = f > 0.6 && (x + y) % 2 === 0 && i + 1 < SKY.length;
      p.px(x, y, SKY[next ? i + 1 : i]!);
    }
  }
  for (let i = 0; i < 26; i++) {
    const x = Math.floor(noise(i, 1, 11) * BACKDROP_W), y = Math.floor(noise(i, 2, 11) * 22);
    p.px(x, y, i % 4 ? "#8C7FA8" : C.paper);
  }
  p.disc(262, 15, 7, "#F3E3BC").disc(259, 13, 2, "#E2CFA2").px(264, 18, "#E2CFA2");
}

function skyline(p: Pixels) {
  // Rite Spire in the distance
  p.rect(156, 10, 8, 52, ROOF).poly([[152, 12], [168, 12], [160, 2]], ROOF).rect(158, 14, 4, 4, C.gold).px(159, 15, C.goldLight);
  let x = -6;
  let i = 0;
  while (x < BACKDROP_W) {
    const w = 22 + Math.floor(noise(i, 5, 21) * 20);
    const h = 14 + Math.floor(noise(i, 6, 21) * 16);
    const top = HORIZON - h;
    if (!(x < 170 && x + w > 150)) {
      p.rect(x, top, w, h, ROOF);
      p.poly([[x - 4, top + 1], [x + w + 4, top + 1], [x + w - 2, top - 5], [x + 2, top - 5]], ROOF_EDGE);
      p.px(x - 5, top, ROOF_EDGE).px(x + w + 5, top, ROOF_EDGE);
      for (let wy = top + 4; wy < HORIZON - 4; wy += 7) for (let wx = x + 4; wx < x + w - 4; wx += 8) {
        if (noise(wx, wy, 22) < 0.45) p.rect(wx, wy, 3, 3, noise(wx, wy, 23) < 0.5 ? C.gold : "#B9772E");
      }
    }
    x += w + 3;
    i++;
  }
}

function stalls(p: Pixels) {
  // left stall
  p.rect(0, 30, 74, 4, C.wood);
  for (let x = 0; x < 78; x += 6) p.poly([[x, 33], [x + 6, 33], [x + 6, 46], [x + 3, 49], [x, 46]], Math.floor(x / 6) % 2 ? C.paper : C.crimson);
  p.rect(4, 46, 3, 30, C.wood).rect(66, 46, 3, 30, C.wood);
  p.rect(0, 60, 74, 12, C.woodLight).rect(0, 60, 74, 2, C.wood);
  for (let k = 0; k < 4; k++) p.rect(8 + k * 15, 52, 11, 8, [C.dread, C.sorrowDark, C.crimsonDark, C.wood][k]!).rect(8 + k * 15, 58, 11, 2, C.paper);
  // right stall
  p.rect(256, 34, 64, 4, C.wood);
  for (let x = 256; x < 322; x += 6) p.poly([[x, 37], [x + 6, 37], [x + 6, 48], [x + 3, 51], [x, 48]], Math.floor(x / 6) % 2 ? C.paper : C.sorrowDark);
  p.rect(260, 48, 3, 24, C.wood).rect(314, 48, 3, 24, C.wood);
  p.rect(256, 58, 64, 10, C.woodLight).rect(256, 58, 64, 2, C.wood);
  for (let k = 0; k < 5; k++) p.disc(268 + k * 10, 55, 3, C.gold).px(268 + k * 10, 55, C.goldDark);
  // hanging talisman strips and lanterns on two cords
  for (let x = 0; x < BACKDROP_W; x++) {
    const y1 = Math.round(18 + Math.sin((x / BACKDROP_W) * Math.PI) * 12);
    p.px(x, y1, C.ink);
    if (x % 26 === 13) {
      p.ellipse(x, y1 + 5, 3, 4, x % 52 === 13 ? C.crimson : C.gold).rect(x - 2, y1 + 1, 5, 1, C.ink).px(x, y1 + 10, C.crimson);
      p.px(x - 1, y1 + 4, C.goldLight);
    }
    const y2 = Math.round(6 + Math.sin((x / BACKDROP_W) * Math.PI) * 7);
    p.px(x, y2, "#4A3D5E");
    if (x % 17 === 8) p.rect(x - 1, y2 + 1, 3, 8, C.paper).px(x, y2 + 3, C.crimson).px(x, y2 + 6, C.crimson);
  }
}

function ground(p: Pixels) {
  for (let y = HORIZON; y < BACKDROP_H; y++) for (let x = 0; x < BACKDROP_W; x++) p.px(x, y, STONE);
  // horizontal grout lines get further apart toward the viewer
  const rows: number[] = [];
  for (let k = 0, y = HORIZON; y < BACKDROP_H; k++) { rows.push(y); y += 3 + k * 2; }
  for (const y of rows) p.rect(0, y, BACKDROP_W, 1, GROUT);
  // vertical lines converge on a vanishing point above the plaza
  const vx = 160, vy = 10;
  for (let k = -14; k <= 14; k++) {
    const bx = 160 + k * 34;
    for (let y = HORIZON; y < BACKDROP_H; y++) {
      const t = (y - vy) / (BACKDROP_H - vy);
      p.px(Math.round(vx + (bx - vx) * t), y, GROUT);
    }
  }
  // alternate tile tone and grain
  for (let y = HORIZON + 1; y < BACKDROP_H; y++) for (let x = 0; x < BACKDROP_W; x++) {
    if (p.get(x, y) !== STONE) continue;
    const n = noise(x, y, 31);
    if (n < 0.05) p.px(x, y, STONE_ALT);
    else if (n > 0.985) p.px(x, y, GROUT);
  }
  p.rect(0, HORIZON, BACKDROP_W, 1, "#8F7A55");
  // scattered coins and receipts
  for (let i = 0; i < 18; i++) {
    const x = Math.floor(noise(i, 7, 41) * BACKDROP_W), y = HORIZON + 6 + Math.floor(noise(i, 8, 41) * 60);
    if (i % 3) p.disc(x, y, 1.5, C.gold).px(x, y, C.goldDark);
    else p.rect(x, y, 4, 2, C.white).px(x + 1, y, C.paperDark);
  }
}

function bindingCircle(p: Pixels, s: { x: number; y: number; rx: number; ry: number }, accent: string) {
  p.ellipse(s.x, s.y + 1, s.rx * 0.8, s.ry * 0.7, "#CDB98F");
  ellipseRing(p, s.x, s.y, s.rx, s.ry, 1.2, C.ink, 4);
  ellipseRing(p, s.x, s.y, s.rx * 0.78, s.ry * 0.78, 1, accent);
  for (let k = 0; k < 8; k++) {
    const a = (k / 8) * Math.PI * 2;
    const x = Math.round(s.x + Math.cos(a) * s.rx * 0.9), y = Math.round(s.y + Math.sin(a) * s.ry * 0.9);
    p.rect(x - 1, y, 3, 1, C.crimson);
  }
}

export function backdropTexture(scene: Phaser.Scene): string {
  const key = "battle-backdrop";
  if (scene.textures.exists(key)) return key;
  const p = new Pixels(BACKDROP_W, BACKDROP_H);
  sky(p);
  skyline(p);
  ground(p);
  stalls(p);
  bindingCircle(p, WILD_SPOT, C.gold);
  bindingCircle(p, PLAYER_SPOT, C.gold);
  makePixelTexture(scene, key, p);
  return key;
}

/**
 * Character and effect textures, drawn in code (no asset files).
 * The player's look lives in playerArt.ts; Kessa and the icons in pixelArt.ts.
 */
import * as Phaser from "phaser";
import { INK, KESSA, drawGrid, drawPixelMap, mapSize, type PixelMap } from "./pixelArt";
import { PLAYER_FRAME, PLAYER_FRAMES_PER_ROW, PLAYER_ROW, playerFrame } from "./playerArt";

export { PLAYER_FRAME, PLAYER_FRAMES_PER_ROW, PLAYER_ROW };

/** A townsperson in a straw hat and a blue work coat (the guild's porters). */
const PORTER: PixelMap = {
  outline: true,
  palette: { h: "#D8B56A", H: "#B8934A", s: "#E8B98E", e: INK, b: "#2F4A7A", B: "#213656", g: "#C8402A", o: "#4A2E1E" },
  rows: [
    ".....hhhh.....",
    "...hhHHHHhh...",
    "hhhhhhhhhhhhhh",
    "...ssssssss...",
    "...sesssses...",
    "...ssssssss...",
    "..bbbbbbbbbb..",
    ".bbbbBbbbbbbb.",
    ".sbbbBbbbbbbs.",
    "..bbbBbbbbbb..",
    "..gggggggggg..",
    "..bbbBbbbbbb..",
    "..bbbb..bbbb..",
    "..oooo..oooo..",
  ],
};

function radial(scene: Phaser.Scene, key: string, size: number, stops: [number, string][]) {
  const t = scene.textures.createCanvas(key, size, size);
  if (!t) return;
  const c = t.getContext();
  const grad = c.createRadialGradient(size / 2, size / 2, 0, size / 2, size / 2, size / 2);
  for (const [at, color] of stops) grad.addColorStop(at, color);
  c.fillStyle = grad;
  c.fillRect(0, 0, size, size);
  t.refresh();
}

function canvasTexture(scene: Phaser.Scene, key: string, w: number, h: number, draw: (c: CanvasRenderingContext2D) => void) {
  const t = scene.textures.createCanvas(key, w, h);
  if (!t) return;
  draw(t.getContext());
  t.refresh();
}

export function buildSpriteTextures(scene: Phaser.Scene) {
  // Player sheet: 3 rows (down, up, side) x 3 frames.
  const F = PLAYER_FRAME;
  const sheet = scene.textures.createCanvas("player", F * PLAYER_FRAMES_PER_ROW, F * 3);
  if (sheet) {
    const ctx = sheet.getContext();
    (Object.keys(PLAYER_ROW) as (keyof typeof PLAYER_ROW)[]).forEach((row) => {
      const r = PLAYER_ROW[row];
      for (let f = 0; f < PLAYER_FRAMES_PER_ROW; f++) {
        drawGrid(ctx, playerFrame(row, f), f * F, r * F);
        sheet.add(r * PLAYER_FRAMES_PER_ROW + f, 0, f * F, r * F, F, F);
      }
    });
    sheet.refresh();
  }

  const k = mapSize(KESSA);
  canvasTexture(scene, "kessa", k.w, k.h, (c) => drawPixelMap(c, KESSA, 0, 0));
  const p = mapSize(PORTER);
  canvasTexture(scene, "porter", p.w, p.h, (c) => drawPixelMap(c, PORTER, 0, 0));

  // Small hanging lantern and a paper talisman flag for the strings over the street.
  canvasTexture(scene, "mini-lantern", 8, 11, (c) => {
    c.fillStyle = "#4A2E1E"; c.fillRect(3, 0, 2, 1);
    c.fillStyle = "#D9A532"; c.fillRect(2, 1, 4, 1); c.fillRect(2, 8, 4, 1);
    c.fillStyle = "#C8402A"; c.fillRect(1, 2, 6, 6);
    c.fillStyle = "#F7C35C"; c.fillRect(2, 3, 4, 4);
    c.fillStyle = "#FFF1C4"; c.fillRect(3, 4, 2, 2);
    c.fillStyle = "#B0153A"; c.fillRect(3, 9, 2, 2);
  });
  canvasTexture(scene, "flag", 7, 9, (c) => {
    c.fillStyle = "#F3E9D2"; c.fillRect(0, 0, 7, 7); c.fillRect(1, 7, 2, 1); c.fillRect(4, 7, 2, 1); c.fillRect(1, 8, 1, 1); c.fillRect(5, 8, 1, 1);
    c.fillStyle = "#B0153A"; c.fillRect(3, 1, 1, 1); c.fillRect(2, 2, 3, 1); c.fillRect(1, 3, 5, 1); c.fillRect(2, 4, 3, 1); c.fillRect(3, 5, 1, 1);
  });

  // Ambient paper scraps, a coin glint and grass leaves.
  canvasTexture(scene, "scrap", 6, 4, (c) => {
    c.fillStyle = "#F3E9D2"; c.fillRect(0, 0, 6, 4);
    c.fillStyle = "#8A7A60"; c.fillRect(1, 1, 4, 1);
  });
  canvasTexture(scene, "scrap-red", 4, 6, (c) => {
    c.fillStyle = "#F3E9D2"; c.fillRect(0, 0, 4, 6);
    c.fillStyle = "#B0153A"; c.fillRect(1, 1, 2, 4);
  });
  canvasTexture(scene, "leaf", 3, 2, (c) => {
    c.fillStyle = "#8DB35E"; c.fillRect(0, 0, 3, 2);
  });
  canvasTexture(scene, "shadow", 18, 6, (c) => {
    c.fillStyle = "rgba(26,23,38,0.35)";
    c.fillRect(3, 0, 12, 6); c.fillRect(0, 1, 18, 4);
  });

  // Light: a soft white brush (cuts holes in the night) and a warm additive glow.
  radial(scene, "light", 128, [[0, "rgba(255,255,255,1)"], [0.45, "rgba(255,255,255,0.75)"], [1, "rgba(255,255,255,0)"]]);
  radial(scene, "glow", 64, [[0, "rgba(255,196,110,0.9)"], [0.4, "rgba(255,150,70,0.35)"], [1, "rgba(255,120,60,0)"]]);

  // Vignette (screen-sized, ink edges).
  const vw = 480, vh = 270;
  canvasTexture(scene, "vignette", vw, vh, (c) => {
    const g = c.createRadialGradient(vw / 2, vh / 2, vh * 0.35, vw / 2, vh / 2, vw * 0.62);
    g.addColorStop(0, "rgba(26,23,38,0)");
    g.addColorStop(1, "rgba(26,23,38,0.85)");
    c.fillStyle = g;
    c.fillRect(0, 0, vw, vh);
  });
}

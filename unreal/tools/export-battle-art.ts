/**
 * Writes the web client's battle art (client/game/battle: the 12 curses, the night-market backdrop, action
 * and emotion icons, talismans, the wax seal, Kessa) as PNG files for the Unreal battle screen.
 * The art is painted by the same code the web client runs, so both clients show the same curses.
 *
 * Run from the repository root (needs node_modules: npm install):
 *   node_modules/.bin/esbuild unreal/tools/export-battle-art.ts --bundle --platform=node \
 *     --alias:phaser=./unreal/tools/phaser-stub.js --outfile=unreal/GrudgeBound/Saved/export-battle-art.cjs
 *   node unreal/GrudgeBound/Saved/export-battle-art.cjs
 * Then import the PNGs: unreal/tools/import-battle-art.py (see its header).
 */
import { writeFileSync, mkdirSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { join } from "node:path";
import { CURSE_SPRITE_IDS, curseTexture } from "../../client/game/battle/curseSprites";
import { backdropTexture } from "../../client/game/battle/backdrop";
import { emotionIcon, iconTexture, kessaTexture, omenPip, stampTexture, talismanTexture, waxSealTexture } from "../../client/game/battle/art";

const OUT = join(process.cwd(), "unreal/GrudgeBound/Content/Grudgebound/UI/Battle");

/** Just enough of a Phaser scene: canvas textures that remember what fillRect painted. */
class Canvas {
  readonly rgba: Uint8Array;
  private color: [number, number, number] = [0, 0, 0];
  imageSmoothingEnabled = false;
  constructor(readonly w: number, readonly h: number) { this.rgba = new Uint8Array(w * h * 4); }
  set fillStyle(hex: string) {
    const n = parseInt(hex.replace("#", ""), 16);
    this.color = [(n >> 16) & 255, (n >> 8) & 255, n & 255];
  }
  fillRect(x: number, y: number, w: number, h: number) {
    for (let j = Math.max(0, y); j < Math.min(this.h, y + h); j++) {
      for (let i = Math.max(0, x); i < Math.min(this.w, x + w); i++) {
        this.rgba.set([...this.color, 255], (j * this.w + i) * 4);
      }
    }
  }
}
const canvases = new Map<string, Canvas>();
const scene = {
  textures: {
    exists: (key: string) => canvases.has(key),
    createCanvas: (key: string, w: number, h: number) => {
      const canvas = new Canvas(w, h);
      canvases.set(key, canvas);
      return { getContext: () => canvas, refresh: () => undefined };
    },
  },
} as never;

function crc32(bytes: Uint8Array): number {
  let c = ~0;
  for (const b of bytes) {
    c ^= b;
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return ~c >>> 0;
}

function png(canvas: Canvas): Buffer {
  const chunk = (type: string, data: Uint8Array) => {
    const body = Buffer.concat([Buffer.from(type, "ascii"), data]);
    const head = Buffer.alloc(4); head.writeUInt32BE(data.length);
    const tail = Buffer.alloc(4); tail.writeUInt32BE(crc32(body));
    return Buffer.concat([head, body, tail]);
  };
  const header = Buffer.alloc(13);
  header.writeUInt32BE(canvas.w, 0); header.writeUInt32BE(canvas.h, 4);
  header.set([8, 6, 0, 0, 0], 8); // 8 bits, RGBA
  const rows = Buffer.alloc((canvas.w * 4 + 1) * canvas.h);
  for (let y = 0; y < canvas.h; y++) rows.set(canvas.rgba.subarray(y * canvas.w * 4, (y + 1) * canvas.w * 4), y * (canvas.w * 4 + 1) + 1);
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", header), chunk("IDAT", deflateSync(rows)), chunk("IEND", new Uint8Array())]);
}

for (const id of CURSE_SPRITE_IDS) curseTexture(scene, id);
backdropTexture(scene);
for (const id of ["attack", "ward", "seal", "salve", "gilded", "flee"] as const) iconTexture(scene, id);
for (const emotion of ["rage", "sorrow", "dread"] as const) emotionIcon(scene, emotion);
omenPip(scene, true); omenPip(scene, false);
talismanTexture(scene, false); talismanTexture(scene, true);
waxSealTexture(scene); stampTexture(scene); kessaTexture(scene);

mkdirSync(OUT, { recursive: true });
for (const [key, canvas] of canvases) {
  // Unreal asset names: letters, digits and underscores.
  const name = key.replace(/^battle-/, "").replace(/[^A-Za-z0-9]/g, "_");
  writeFileSync(join(OUT, `${name}.png`), png(canvas));
  console.log(`${name}.png ${canvas.w}x${canvas.h}`);
}

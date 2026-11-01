/**
 * Procedural pixel-art battle sprites for the 12 curses. Every curse is made of a town object
 * (coin, purse, bell, lantern, candle, shutter, ledger, rule book...) and carries its emotion's
 * colour. Whisper sprites are 48px, Wail 56px, Calamity 64px; the scene scales them up 3-4x.
 */
import type * as Phaser from "phaser";
import { Pixels, C, makePixelTexture } from "./pixels";

type Painter = (p: Pixels) => void;
type EyeStyle = "angry" | "sad" | "glow" | "plain";

/** 2x2 eye with a glint, plus brows that set the mood. (x, y) is the eye's top-left. */
function eye(p: Pixels, x: number, y: number, style: EyeStyle, side: "l" | "r", glow: string = C.white) {
  if (style === "glow") {
    p.rect(x - 1, y - 1, 4, 4, C.ink).rect(x, y, 2, 2, glow).px(x + (side === "l" ? 1 : 0), y, C.white);
    return;
  }
  p.rect(x, y, 2, 3, C.ink).px(x, y, C.white);
  if (style === "angry") {
    // brow slopes down toward the nose
    if (side === "l") p.line(x - 2, y - 3, x + 2, y - 1, C.ink);
    else p.line(x + 3, y - 3, x - 1, y - 1, C.ink);
  } else if (style === "sad") {
    if (side === "l") p.line(x - 1, y - 1, x + 2, y - 3, C.ink);
    else p.line(x + 2, y - 1, x - 1, y - 3, C.ink);
  }
}

function inkLines(p: Pixels, x: number, y: number, w: number, rows: number, step = 2, c: string = C.inkSoft) {
  for (let r = 0; r < rows; r++) p.rect(x, y + r * step, Math.max(1, w - ((r * 3) % 3)), 1, c);
}

// ------------------------------------------------------------------ RAGE

/** A floating ember-hot coin with a square hole and a jagged grin. */
const cinderGrin: Painter = (p) => {
  for (const [x0, top, x1] of [[15, 3, 21], [20, 0, 28], [27, 3, 33]] as const) {
    p.poly([[x0, 13], [(x0 + x1) / 2, top], [x1, 13]], C.rageLight);
  }
  p.disc(24, 25, 16, C.rageLight).disc(24, 25, 14, C.gold);
  p.recolor(C.gold, C.goldDark, (x, y) => (x - 24) + (y - 25) > 13);
  p.recolor(C.gold, C.goldLight, (x, y) => (x - 24) + (y - 25) < -15);
  p.ring(24, 25, 11, 1, C.goldDark);
  p.rect(21, 12, 6, 6, null); // square hole of the coin
  p.outline();
  for (const [x0, top, x1] of [[17, 6, 20], [22, 3, 26], [28, 6, 31]] as const) p.poly([[x0, 13], [(x0 + x1) / 2, top], [x1, 13]], C.ember);
  eye(p, 15, 21, "angry", "l");
  eye(p, 31, 21, "angry", "r");
  p.poly([[12, 28], [36, 28], [32, 35], [16, 35]], C.mouth);
  for (let x = 13; x < 35; x += 4) p.poly([[x, 28], [x + 4, 28], [x + 2, 31]], C.ember);
  for (let x = 17; x < 31; x += 4) p.poly([[x, 35], [x + 4, 35], [x + 2, 33]], C.ember);
  for (const [x, y] of [[5, 15], [42, 12], [3, 30], [44, 33], [39, 4], [8, 6]] as const) p.px(x, y, C.ember).px(x + 1, y + 1, C.rageLight);
};

/** A dented iron coin-purse with a bandaged fist. */
const knucklebrat: Painter = (p) => {
  p.ellipse(21, 31, 13, 11, C.iron);
  p.poly([[14, 21], [28, 21], [26, 15], [16, 15]], C.iron);
  for (let x = 14; x < 29; x += 3) p.poly([[x, 15], [x + 3, 15], [x + 1.5, 11]], C.ironLight);
  p.rect(30, 27, 5, 4, C.iron); // arm
  p.rect(34, 23, 10, 10, C.ironLight); // fist
  p.rect(13, 41, 6, 4, C.iron).rect(24, 41, 6, 4, C.iron);
  p.outline();
  p.recolor(C.iron, "#38364A", (x, y) => (x - 21) + (y - 31) > 9);
  p.recolor(C.iron, C.ironLight, (x, y) => (x - 21) + (y - 31) < -14 && y > 20);
  p.rect(13, 19, 16, 2, C.rage).rect(26, 17, 3, 5, C.rage).line(28, 22, 31, 26, C.rage).line(27, 22, 28, 27, C.rage);
  p.rect(34, 25, 10, 2, C.paper).rect(34, 29, 10, 2, C.paper);
  p.px(37, 23, C.ink).px(40, 23, C.ink).px(37, 24, C.ink).px(40, 24, C.ink);
  p.px(43, 28, C.crimson);
  eye(p, 15, 27, "angry", "l");
  eye(p, 24, 27, "angry", "r");
  p.rect(16, 34, 10, 3, C.mouth).px(17, 34, C.paper).px(19, 34, C.paper).px(21, 34, C.paper).px(23, 34, C.paper).px(25, 34, C.paper);
  p.line(10, 29, 12, 33, C.inkSoft).line(28, 36, 30, 38, C.inkSoft); // dents
  p.rect(9, 36, 4, 3, C.rage).px(10, 37, C.rageLight); // patch
  p.disc(5, 43, 2, C.gold).px(5, 43, C.goldDark);
};

/** A crab made of a snapping coin purse; claws are clasp locks; scam contracts in its mouth. */
const gutterjaw: Painter = (p) => {
  const leather = "#9A3326";
  for (let i = 0; i < 3; i++) {
    p.line(18 + i * 4, 40, 12 + i * 3, 50, C.rageDark).line(19 + i * 4, 40, 13 + i * 3, 50, C.rageDark);
    p.line(38 - i * 4, 40, 44 - i * 3, 50, C.rageDark).line(37 - i * 4, 40, 43 - i * 3, 50, C.rageDark);
  }
  p.ellipse(28, 32, 18, 11, leather);
  p.rect(5, 27, 8, 4, leather).rect(43, 27, 8, 4, leather);
  p.poly([[1, 13], [10, 12], [9, 19], [4, 20]], C.gold).poly([[1, 25], [10, 26], [9, 20], [4, 19]], C.gold);
  p.poly([[55, 13], [46, 12], [47, 19], [52, 20]], C.gold).poly([[55, 25], [46, 26], [47, 20], [52, 19]], C.gold);
  p.rect(7, 20, 3, 8, leather).rect(46, 20, 3, 8, leather);
  p.line(20, 22, 18, 12, C.rageDark).line(21, 22, 19, 12, C.rageDark).disc(18, 10, 3, C.paper);
  p.line(36, 22, 38, 12, C.rageDark).line(35, 22, 37, 12, C.rageDark).disc(38, 10, 3, C.paper);
  p.rect(22, 18, 12, 3, C.goldDark).disc(24, 19, 2, C.gold).disc(32, 19, 2, C.gold);
  p.poly([[23, 34], [33, 33], [35, 47], [25, 48]], C.paper);
  p.outline();
  p.recolor(leather, C.rageDark, (x, y) => y > 37);
  p.recolor(leather, C.rage, (x, y) => y < 26 && x < 30);
  p.poly([[12, 30], [44, 30], [39, 37], [17, 37]], C.mouth);
  for (let x = 13; x < 43; x += 3) p.rect(x, 30, 2, 2, C.gold);
  p.poly([[23, 34], [33, 33], [35, 47], [25, 48]], C.paper);
  p.line(23, 34, 25, 48, C.ink).line(33, 33, 35, 47, C.ink);
  for (let r = 0; r < 5; r++) p.line(25, 37 + r * 2, 32, 36 + r * 2, C.inkSoft);
  p.px(30, 45, C.crimson).px(31, 45, C.crimson);
  p.rect(18, 9, 2, 2, C.ink).rect(37, 9, 2, 2, C.ink).px(18, 9, C.white).px(37, 9, C.white);
  p.line(15, 6, 20, 8, C.ink).line(41, 6, 36, 8, C.ink);
  p.recolor(C.gold, C.goldDark, (x, y) => y > 21 && (x < 11 || x > 45));
};

/** A huge cracked temple bell with a furious face, chains and torn banners. */
const riotBell: Painter = (p) => {
  for (let y = 0; y < 10; y += 3) p.rect(30, y, 4, 2, y % 2 ? C.ironLight : C.iron).rect(31, y + 2, 2, 1, C.iron);
  p.rect(26, 9, 12, 4, C.goldDark);
  p.ellipse(32, 18, 13, 6, C.gold);
  p.poly([[19, 17], [45, 17], [48, 32], [51, 46], [56, 51], [8, 51], [13, 46], [16, 32]], C.gold);
  p.ellipse(32, 52, 26, 4, C.goldDark);
  p.disc(32, 58, 4, C.iron);
  p.poly([[16, 22], [5, 26], [7, 41], [10, 37], [12, 43], [17, 31]], C.crimson);
  p.poly([[48, 22], [59, 26], [57, 41], [54, 37], [52, 43], [47, 31]], C.crimson);
  p.outline();
  p.recolor(C.gold, C.goldDark, (x) => x > 42);
  p.recolor(C.gold, C.goldLight, (x, y) => x >= 18 && x <= 20 && y > 16 && y < 48);
  p.recolor(C.gold, C.goldDark, (x, y) => y === 22 || y === 45);
  p.line(40, 15, 37, 22, C.ink).line(37, 22, 41, 28, C.ink).line(41, 28, 36, 36, C.ink).line(36, 36, 38, 40, C.ink);
  p.rect(23, 29, 4, 4, C.ink).px(23, 29, C.white).rect(38, 29, 4, 4, C.ink).px(38, 29, C.white);
  p.line(19, 24, 27, 28, C.ink).line(19, 25, 27, 29, C.ink).line(46, 24, 38, 28, C.ink).line(46, 25, 38, 29, C.ink);
  p.rect(24, 36, 17, 8, C.mouth).rect(27, 40, 11, 3, C.rage);
  for (let x = 24; x < 41; x += 3) p.rect(x, 36, 2, 2, C.paper);
  for (let x = 25; x < 40; x += 4) p.rect(x, 43, 2, 1, C.paper);
  p.rect(7, 30, 3, 2, C.crimsonDark).rect(8, 34, 2, 2, C.crimsonDark).rect(54, 30, 3, 2, C.crimsonDark).rect(55, 34, 2, 2, C.crimsonDark);
  for (const s of [1, -1]) {
    const x = s > 0 ? 61 : 2;
    p.line(x, 28, x + s * 2, 38, C.rageLight).line(x + s * 2, 38, x, 48, C.rageLight);
  }
};

// ------------------------------------------------------------------ SORROW

/** A jellyfish made of a soggy paper lantern; tentacles are wet receipts dripping ink. */
const lanternDrowned: Painter = (p) => {
  const strips = [10, 16, 22, 28, 34];
  const lens = [16, 20, 18, 21, 15];
  strips.forEach((x0, k) => {
    for (let y = 22; y < 22 + lens[k]!; y++) p.rect(x0 + Math.round(Math.sin((y + k * 4) / 3) * 1.4), y, 4, 1, C.paper);
  });
  p.ellipse(24, 17, 15, 11, C.paper);
  p.rect(6, 23, 36, 6, null);
  for (let k = 0; k < strips.length; k++) for (let y = 23; y < 25; y++) p.rect(strips[k]! + Math.round(Math.sin((y + k * 4) / 3) * 1.4), y, 4, 1, C.paper);
  p.rect(18, 4, 12, 3, C.sorrowDark);
  p.rect(9, 21, 30, 2, C.sorrowDark);
  p.outline();
  p.recolor(C.paper, C.paperShade, (x, y) => y < 21 && (x === 14 || x === 24 || x === 34));
  p.recolor(C.paper, C.sorrowLight, (x, y) => y > 15 && y < 21);
  p.recolor(C.paper, "#C9DDF0", (x, y) => y >= 24 && y % 5 === 0);
  strips.forEach((x0, k) => {
    for (let y = 26; y < 20 + lens[k]!; y += 3) p.px(x0 + 1 + Math.round(Math.sin((y + k * 4) / 3) * 1.4), y, C.inkSoft).px(x0 + 2 + Math.round(Math.sin((y + k * 4) / 3) * 1.4), y, C.inkSoft);
    const ex = x0 + 1 + Math.round(Math.sin((22 + lens[k]! + k * 4) / 3) * 1.4);
    p.px(ex, 23 + lens[k]!, C.sorrow).px(ex, 25 + lens[k]!, C.sorrowLight);
  });
  eye(p, 17, 12, "sad", "l");
  eye(p, 29, 12, "sad", "r");
  p.px(16, 16, C.sorrow).px(16, 17, C.sorrow).px(31, 16, C.sorrow);
  p.rect(22, 18, 4, 1, C.ink).px(21, 19, C.ink).px(26, 19, C.ink);
  p.px(23, 8, C.goldLight).px(24, 8, C.gold);
};

/** A melting candle with a crying face and an unpaid wage slip stuck to the wax. */
const weepwick: Painter = (p) => {
  p.ellipse(24, 43, 14, 3, C.goldDark);
  p.rect(15, 18, 18, 25, C.wax);
  p.ellipse(24, 18, 9, 2, C.wax);
  p.rect(13, 20, 3, 8, C.wax).disc(14, 28, 1.5, C.wax);
  p.rect(32, 19, 3, 11, C.wax).disc(33, 30, 1.5, C.wax);
  p.rect(23, 11, 2, 7, C.ink);
  p.ellipse(24, 8, 3, 4, C.sorrowLight).poly([[21, 7], [24, 0], [27, 7]], C.sorrowLight);
  p.poly([[27, 30], [37, 28], [38, 37], [28, 39]], C.paper);
  p.outline();
  p.recolor(C.wax, C.waxShade, (x) => x > 28);
  p.recolor(C.goldDark, C.gold, (x, y) => y < 43);
  p.ellipse(24, 9, 1, 2, C.white).px(24, 5, C.sorrow);
  for (let r = 0; r < 4; r++) p.line(29, 32 + r * 2, 36, 30 + r * 2, C.inkSoft);
  p.px(28, 31, C.crimson);
  eye(p, 18, 23, "sad", "l");
  eye(p, 26, 23, "sad", "r");
  p.rect(18, 27, 1, 7, C.sorrowLight).rect(27, 27, 1, 3, C.sorrowLight).px(18, 34, C.sorrow);
  p.rect(21, 31, 3, 2, C.mouth);
};

/** A tall wax figure under a veil of IOU notes, holding an empty purse. */
const tallowWidow: Painter = (p) => {
  p.poly([[20, 24], [36, 24], [43, 52], [13, 52]], C.wax);
  for (const x of [15, 22, 31, 38]) p.rect(x, 51, 2, 3, C.wax);
  p.ellipse(28, 16, 7, 8, C.wax);
  p.line(19, 28, 12, 38, C.wax).line(20, 28, 13, 38, C.wax).line(21, 29, 14, 38, C.wax);
  const notes: [number, number][][] = [
    [[18, 9], [27, 6], [28, 14], [19, 16]], [[28, 5], [37, 8], [36, 16], [27, 14]],
    [[16, 15], [23, 14], [24, 27], [16, 28]], [[33, 14], [40, 15], [40, 29], [32, 27]],
    [[21, 3], [33, 2], [33, 7], [21, 8]],
  ];
  notes.forEach((n, i) => p.poly(n, i % 2 ? C.paperShade : C.paper));
  p.line(12, 38, 11, 40, C.wood).ellipse(11, 43, 4, 4, C.wood);
  p.outline();
  p.recolor(C.wax, C.waxShade, (x, y) => y > 24 && x > 31);
  p.recolor(C.wax, C.sorrow, (x, y) => y === 31 || y === 32);
  p.recolor(C.wood, C.woodLight, (x, y) => y < 42 && x < 11);
  p.rect(9, 40, 5, 1, C.ink);
  for (const [x, y] of [[20, 10], [30, 8], [18, 18], [34, 18], [24, 4]] as const) p.rect(x, y, 4, 1, C.inkSoft).rect(x, y + 2, 3, 1, C.inkSoft);
  p.disc(36, 24, 1.5, C.crimson).disc(19, 25, 1.2, C.crimson);
  eye(p, 25, 15, "sad", "l");
  eye(p, 30, 15, "sad", "r");
  p.px(25, 19, C.sorrowLight).px(25, 20, C.sorrowLight).px(31, 19, C.sorrowLight);
  p.rect(27, 21, 3, 1, C.ink);
  p.px(24, 40, C.waxShade).px(24, 41, C.waxShade).px(33, 44, C.waxShade);
};

/** A shop's wooden shutter come alive, mouth full of receipts and coins, hung with closed tags. */
const theShuttered: Painter = (p) => {
  p.poly([[3, 9], [61, 9], [56, 2], [8, 2]], C.crimson);
  p.rect(8, 9, 48, 50, C.wood);
  p.rect(12, 59, 7, 4, C.wood).rect(45, 59, 7, 4, C.wood);
  p.line(9, 12, 5, 28, C.ink).rect(1, 28, 7, 9, C.paper);
  p.line(55, 14, 59, 34, C.ink).rect(56, 34, 7, 9, C.paper);
  p.outline();
  p.recolor(C.crimson, C.paper, (x) => Math.floor(x / 6) % 2 === 0);
  p.rect(11, 12, 42, 44, C.woodLight);
  for (let y = 15; y < 56; y += 4) p.rect(11, y, 42, 1, C.wood);
  p.poly([[12, 31], [52, 31], [47, 45], [17, 45]], C.mouth);
  for (let x = 13; x < 51; x += 5) p.poly([[x, 31], [x + 4, 31], [x + 2, 35]], C.woodLight);
  for (let x = 19; x < 46; x += 5) p.poly([[x, 45], [x + 4, 45], [x + 2, 42]], C.woodLight);
  p.poly([[22, 36], [28, 35], [29, 44], [23, 44]], C.paper).poly([[33, 37], [39, 38], [38, 44], [32, 44]], C.paperShade);
  p.disc(30, 40, 2, C.gold).disc(42, 40, 2, C.gold).px(30, 40, C.ink);
  p.rect(18, 19, 9, 4, C.ink).rect(20, 20, 4, 2, C.sorrowLight).px(20, 20, C.white);
  p.rect(37, 19, 9, 4, C.ink).rect(40, 20, 4, 2, C.sorrowLight).px(43, 20, C.white);
  p.line(17, 17, 27, 18, C.ink).line(47, 17, 37, 18, C.ink);
  p.rect(3, 31, 3, 3, C.crimson).rect(58, 37, 3, 3, C.crimson);
  p.rect(8, 9, 48, 1, C.ink);
  p.px(24, 58, C.paper).px(25, 58, C.paper).rect(36, 57, 3, 2, C.paper);
};

// ------------------------------------------------------------------ DREAD

/** A moth with paper wings stamped with red seals and a glowing eye on each wing. */
const hushmoth: Painter = (p) => {
  p.poly([[23, 21], [4, 5], [1, 23], [20, 27]], C.paper);
  p.poly([[25, 21], [44, 5], [47, 23], [28, 27]], C.paper);
  p.poly([[22, 26], [7, 32], [11, 42], [23, 33]], C.paperShade);
  p.poly([[26, 26], [41, 32], [37, 42], [25, 33]], C.paperShade);
  p.ellipse(24, 27, 3, 11, C.dreadDark);
  p.disc(24, 15, 4, C.dreadDark);
  p.line(22, 12, 17, 3, C.dreadDark).line(26, 12, 31, 3, C.dreadDark);
  p.rect(15, 2, 3, 2, C.dreadDark).rect(31, 2, 3, 2, C.dreadDark);
  p.outline();
  p.recolor(C.paper, C.dreadLight, (x) => x < 7 || x > 41);
  p.recolor(C.paperShade, C.dreadLight, (x, y) => y > 38);
  p.line(22, 21, 6, 9, C.paperDark).line(26, 21, 42, 9, C.paperDark);
  p.disc(12, 17, 3, C.dreadLight).disc(12, 17, 1.5, C.white).px(12, 17, C.dread);
  p.disc(36, 17, 3, C.dreadLight).disc(36, 17, 1.5, C.white).px(36, 17, C.dread);
  p.disc(14, 35, 2.2, C.crimson).px(14, 35, C.crimsonLight).disc(34, 35, 2.2, C.crimson).px(34, 35, C.crimsonLight);
  p.recolor(C.dreadDark, C.dread, (x, y) => x === 23 && y > 18);
  p.px(22, 14, C.dreadLight).px(26, 14, C.dreadLight);
  for (const [x, y] of [[3, 40], [44, 42], [8, 46]] as const) p.px(x, y, C.dreadLight);
};

/** A crooked archive staircase of ledgers, with a mouth on its risers and one eye on top. */
const stairmouth: Painter = (p) => {
  const covers = [C.dread, C.wood, C.dreadDark, C.crimsonDark, C.dread];
  for (let i = 0; i < 5; i++) {
    const x = 5 + i * 7 + (i % 2 ? 1 : -1), y = 37 - i * 7;
    p.rect(x, y, 43 - x, 7, covers[i]!);
  }
  p.rect(1, 5, 5, 4, C.paper).rect(42, 18, 4, 5, C.paper).rect(3, 20, 3, 3, C.paperShade);
  p.disc(41, 6, 4, C.paper);
  p.outline();
  for (let i = 0; i < 5; i++) {
    const x = 5 + i * 7 + (i % 2 ? 1 : -1), y = 37 - i * 7;
    p.rect(x, y + 5, 43 - x, 2, C.paper).rect(x, y + 6, 43 - x, 1, C.paperShade);
    p.rect(x + 2, y + 1, 1, 4, C.gold);
    p.px(x, y, C.ink);
  }
  p.poly([[7, 28], [20, 28], [19, 41], [8, 41]], C.mouth);
  for (let y = 29; y < 41; y += 3) p.rect(8, y, 3, 1, C.paper).rect(16, y + 1, 3, 1, C.paper);
  p.rect(39, 5, 4, 3, C.ink).rect(40, 6, 2, 2, C.dreadLight).px(40, 5, C.white);
  p.line(37, 2, 44, 3, C.ink);
  p.rect(2, 6, 3, 1, C.inkSoft).rect(43, 20, 2, 1, C.inkSoft);
  for (const [x, y] of [[30, 2], [46, 30], [1, 34], [25, 12]] as const) p.px(x, y, C.paperDark);
};

/** A shadow folded from nine sheets of black paper, eyes between the folds. */
const ninefoldShade: Painter = (p) => {
  const f1 = "#221C34", f2 = "#30284C", f3 = C.dreadDark;
  p.poly([[28, 3], [43, 22], [28, 44], [13, 22]], f1);
  p.poly([[13, 22], [1, 31], [17, 39]], f2);
  p.poly([[43, 22], [55, 31], [39, 39]], f2);
  p.poly([[28, 3], [19, 11], [24, 19]], f3);
  p.poly([[28, 3], [37, 11], [32, 19]], f3);
  p.poly([[17, 39], [28, 44], [21, 54], [14, 47]], f2);
  p.poly([[39, 39], [28, 44], [35, 54], [42, 47]], f2);
  p.poly([[13, 22], [6, 13], [18, 16]], f3);
  p.poly([[43, 22], [50, 13], [38, 16]], f3);
  p.outline();
  p.line(28, 4, 28, 43, f2).line(14, 22, 42, 22, f3);
  p.line(13, 22, 17, 38, C.dread).line(43, 22, 39, 38, C.dread).line(28, 4, 19, 11, C.dreadLight).line(28, 4, 37, 11, C.dreadLight);
  p.rect(20, 25, 5, 2, C.dreadLight).rect(21, 25, 2, 1, C.white);
  p.rect(32, 25, 5, 2, C.dreadLight).rect(34, 25, 2, 1, C.white);
  p.rect(27, 33, 3, 2, C.dreadLight).px(28, 33, C.white);
  p.rect(20, 53, 2, 3, C.ink).rect(35, 54, 2, 2, C.ink).px(28, 48, C.ink);
};

/** A giant rule book on ink legs, covered in forbidding stamps, wearing a paper crown. */
const curfewKing: Painter = (p) => {
  p.rect(19, 52, 5, 10, C.ink).rect(17, 61, 9, 2, C.ink);
  p.rect(40, 52, 5, 10, C.ink).rect(38, 61, 9, 2, C.ink);
  p.rect(10, 14, 6, 40, C.dreadDark);
  p.rect(15, 14, 36, 40, C.dread);
  p.rect(50, 16, 5, 37, C.paper);
  p.poly([[16, 15], [16, 4], [22, 10], [27, 1], [32, 9], [37, 1], [42, 10], [48, 4], [48, 15]], C.paper);
  p.outline();
  for (let y = 18; y < 52; y += 2) p.rect(50, y, 5, 1, C.paperShade);
  p.rect(16, 11, 32, 3, C.gold).px(22, 12, C.crimson).px(32, 12, C.crimson).px(42, 12, C.crimson);
  p.rect(10, 14, 6, 40, C.dreadDark).rect(11, 20, 4, 1, C.gold).rect(11, 46, 4, 1, C.gold);
  p.recolor(C.dread, C.dreadDark, (x) => x > 45);
  for (const [x, y] of [[15, 15], [45, 15], [15, 48], [45, 48]] as const) p.rect(x, y, 5, 5, C.gold).px(x === 15 ? x + 4 : x, y === 15 ? y + 4 : y, C.dread);
  for (const [cx, cy, r] of [[23, 45, 4], [41, 44, 4], [43, 21, 3]] as const) {
    p.ring(cx, cy, r, 1, C.crimson).line(cx - r + 1, cy + r - 1, cx + r - 1, cy - r + 1, C.crimson);
  }
  p.line(21, 26, 28, 29, C.ink).line(43, 26, 36, 29, C.ink);
  p.rect(22, 29, 6, 3, C.ink).rect(23, 30, 4, 1, C.dreadLight).px(24, 30, C.white);
  p.rect(36, 29, 6, 3, C.ink).rect(37, 30, 4, 1, C.dreadLight).px(40, 30, C.white);
  p.rect(25, 36, 14, 4, C.mouth);
  for (let x = 25; x < 39; x += 3) p.rect(x, 36, 2, 1, C.paper);
  p.rect(18, 53, 3, 3, C.ink).rect(30, 54, 2, 4, C.ink).px(31, 59, C.ink);
};

// ------------------------------------------------------------------ registry

const PAINTERS: Record<string, { size: number; paint: Painter }> = {
  "cinder-grin": { size: 48, paint: cinderGrin },
  knucklebrat: { size: 48, paint: knucklebrat },
  gutterjaw: { size: 56, paint: gutterjaw },
  "riot-bell": { size: 64, paint: riotBell },
  "lantern-drowned": { size: 48, paint: lanternDrowned },
  weepwick: { size: 48, paint: weepwick },
  "tallow-widow": { size: 56, paint: tallowWidow },
  "the-shuttered": { size: 64, paint: theShuttered },
  hushmoth: { size: 48, paint: hushmoth },
  stairmouth: { size: 48, paint: stairmouth },
  "ninefold-shade": { size: 56, paint: ninefoldShade },
  "curfew-king": { size: 64, paint: curfewKing },
};

/** Builds the curse's texture once and returns its key and source size in pixels. */
export function curseTexture(scene: Phaser.Scene, id: string): { key: string; size: number } {
  const entry = PAINTERS[id] ?? PAINTERS["cinder-grin"]!;
  const key = `curse-${id}`;
  if (!scene.textures.exists(key)) {
    const p = new Pixels(entry.size, entry.size);
    entry.paint(p);
    makePixelTexture(scene, key, p);
  }
  return { key, size: entry.size };
}

export const CURSE_SPRITE_IDS = Object.keys(PAINTERS);

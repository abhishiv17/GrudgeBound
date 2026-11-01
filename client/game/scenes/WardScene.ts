import * as Phaser from "phaser";
import { POINTS_FOR_OMEN, curseById, cursesWhere, rollSpawnEmotion, rollSpawnOmen, createRng } from "@grudgebound/shared";
import { SOLID, T, TILE, buildLanternWard, buildOverLayer } from "../tiles";
import { PLAYER_FRAMES_PER_ROW, PLAYER_ROW } from "../sprites";
import { worldStore } from "../../lib/worldStore";
import { isBlocking, touchInput, uiStore, type BattleRequest } from "../../lib/uiStore";
import { captureVerb, playerStore } from "../../lib/playerStore";
import { BATTLE_END, type BattleData, type BattleResult } from "./BattleScene";

const SPEED = 110;
/** Integer zoom keeps every pixel the same size. */
const ZOOM = 2;
const ENCOUNTER_CHANCE_PER_TILE = 0.12;
/** Tiles of tall grass you can cross after a battle before the next curse may appear. */
const GRACE_TILES = 3;
/** How the town names you in its log until email login gives you a real name. */
const PLAYER_NAME = "A newcomer";
const BOARD = { x: 21 * TILE + TILE / 2, y: 10 * TILE + TILE / 2 };
const NIGHT_COLOR = Phaser.Display.Color.ValueToColor(0x0e0b26);
const DUSK_COLOR = Phaser.Display.Color.ValueToColor(0x5a2a48);
const MAX_DARK = 0.62;

type Facing = keyof typeof PLAYER_ROW;

interface Light {
  x: number;
  y: number;
  r: number;
  seed: number;
  glow?: Phaser.GameObjects.Image;
}

/** How dark the Ward is at a given in-game hour (0 = day, MAX_DARK = deep night). */
function darknessFor(hour: number): number {
  if (hour >= 21 || hour < 5) return MAX_DARK;
  if (hour >= 19) return 0.28 + (hour - 19) * 0.17;
  if (hour >= 17) return (hour - 17) * 0.12;
  if (hour < 7) return MAX_DARK - (hour - 4) * 0.2;
  return 0;
}

/**
 * Lantern Ward: walk the night market, step into tall grass, meet curses.
 * The HUD, Kessa's banner and the encounter dialog are React overlays (components/hud);
 * this scene shares the world feed with them through lib/worldStore and lib/uiStore.
 */
export class WardScene extends Phaser.Scene {
  private player!: Phaser.Physics.Arcade.Sprite;
  private shadow!: Phaser.GameObjects.Image;
  private cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
  private wasd!: Record<"W" | "A" | "S" | "D", Phaser.Input.Keyboard.Key>;
  private layer!: Phaser.Tilemaps.TilemapLayer;
  private facing: Facing = "down";
  private grassDistance = 0;
  private rng = createRng(Date.now() >>> 0);
  private reduceMotion = false;
  private blocked = false;
  private fighting = false;
  private graceTiles = 0;
  private cleanups: (() => void)[] = [];

  // lighting
  private lamps: Light[] = [];
  private nightRt!: Phaser.GameObjects.RenderTexture;
  private brush!: Phaser.GameObjects.Image;
  private vignette!: Phaser.GameObjects.Image;
  private fx = { x: 0, y: 0, w: 0, h: 0 };
  private dark = MAX_DARK;
  private darkTarget = MAX_DARK;
  private leafFx?: Phaser.GameObjects.Particles.ParticleEmitter;

  constructor() {
    super("ward");
  }

  create() {
    this.reduceMotion = typeof window !== "undefined" && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches;
    this.lamps = [];
    this.cleanups = [];
    this.blocked = false;
    this.fighting = false;
    this.graceTiles = 0;

    // --- map
    const data = buildLanternWard();
    const map = this.make.tilemap({ data, tileWidth: TILE, tileHeight: TILE });
    const tileset = map.addTilesetImage("tiles", "tiles", TILE, TILE, 0, 0)!;
    this.layer = map.createLayer(0, tileset, 0, 0)!.setDepth(0);
    this.layer.setCollision(SOLID);
    const overMap = this.make.tilemap({ data: buildOverLayer(data), tileWidth: TILE, tileHeight: TILE });
    const overTiles = overMap.addTilesetImage("tiles", "tiles", TILE, TILE, 0, 0)!;
    overMap.createLayer(0, overTiles, 0, 0)!.setDepth(4);

    // --- player
    this.player = this.physics.add.sprite(21.5 * TILE, 13.5 * TILE, "player", PLAYER_ROW.down * PLAYER_FRAMES_PER_ROW);
    this.player.setSize(14, 10).setOffset(9, 21).setDepth(3);
    this.shadow = this.add.image(this.player.x, this.player.y + 14, "shadow").setDepth(2);
    this.physics.add.collider(this.player, this.layer);
    this.physics.world.setBounds(0, 0, map.widthInPixels, map.heightInPixels);
    this.player.setCollideWorldBounds(true);
    this.createAnimations();

    const cam = this.cameras.main;
    cam.setZoom(ZOOM).setRoundPixels(true);
    cam.setBounds(0, 0, map.widthInPixels, map.heightInPixels);
    cam.startFollow(this.player, true, 0.15, 0.15);

    // --- dressing, lights, ambience
    this.placeProps();
    this.buildLanternStrings();
    this.buildLighting();
    this.buildAmbience();
    this.animateWater();

    // --- input
    const kb = this.input.keyboard!;
    this.cursors = kb.createCursorKeys();
    this.wasd = kb.addKeys("W,A,S,D") as Record<"W" | "A" | "S" | "D", Phaser.Input.Keyboard.Key>;
    kb.on("keydown-E", () => {
      if (!this.blocked && uiStore.getSnapshot().nearBoard) uiStore.openPanel("rules");
    });

    // --- shared state
    this.cleanups.push(worldStore.subscribe(() => this.onWorld()));
    this.cleanups.push(uiStore.subscribe(() => {
      this.syncInputCapture();
      this.maybeStartBattle();
    }));
    const onFocus = () => this.syncInputCapture();
    document.addEventListener("focusin", onFocus);
    document.addEventListener("focusout", onFocus);
    this.cleanups.push(() => {
      document.removeEventListener("focusin", onFocus);
      document.removeEventListener("focusout", onFocus);
    });
    this.onWorld();
    this.syncInputCapture();

    const cleanup = () => {
      for (const c of this.cleanups) c();
      this.cleanups = [];
      uiStore.setNearBoard(false);
    };
    this.events.once(Phaser.Scenes.Events.SHUTDOWN, cleanup);
    this.events.once(Phaser.Scenes.Events.DESTROY, cleanup);
  }

  update(time: number, delta: number) {
    const body = this.player.body as Phaser.Physics.Arcade.Body;

    // ease the night in and out
    this.dark += (this.darkTarget - this.dark) * Math.min(1, delta / 900);
    this.vignette.setAlpha(0.3 + this.dark * 0.7);
    for (const l of this.lamps) {
      if (!l.glow) continue;
      const flicker = this.reduceMotion ? 1 : 1 + Math.sin(time * 0.005 + l.seed) * 0.08;
      l.glow.setAlpha(Math.min(1, this.dark * 1.4) * flicker);
    }

    if (this.blocked) {
      body.setVelocity(0, 0);
      this.idle();
      this.shadow.setPosition(this.player.x, this.player.y + 14);
      return;
    }

    const left = this.cursors.left.isDown || this.wasd.A.isDown || touchInput.left;
    const right = this.cursors.right.isDown || this.wasd.D.isDown || touchInput.right;
    const up = this.cursors.up.isDown || this.wasd.W.isDown || touchInput.up;
    const down = this.cursors.down.isDown || this.wasd.S.isDown || touchInput.down;
    const vx = (right ? 1 : 0) - (left ? 1 : 0);
    const vy = (down ? 1 : 0) - (up ? 1 : 0);
    const len = Math.hypot(vx, vy) || 1;
    body.setVelocity((vx / len) * SPEED, (vy / len) * SPEED);

    if (vx || vy) {
      this.facing = vx ? "side" : vy < 0 ? "up" : "down";
      if (vx) this.player.setFlipX(vx < 0);
      this.player.anims.play(`walk-${this.facing}`, true);
    } else {
      this.idle();
    }
    this.shadow.setPosition(this.player.x, this.player.y + 14);

    uiStore.setNearBoard(Phaser.Math.Distance.Between(this.player.x, this.player.y, BOARD.x, BOARD.y + TILE) < TILE * 1.6);

    // Curses lurk in tall grass: roll once per tile walked.
    const tile = this.layer.getTileAtWorldXY(this.player.x, this.player.y + 10);
    if (tile?.index === T.tallGrass && (vx || vy)) {
      this.grassDistance += (SPEED * delta) / 1000;
      if (this.grassDistance >= TILE) {
        this.grassDistance = 0;
        this.leafFx?.explode(5, this.player.x, this.player.y + 12);
        if (this.graceTiles > 0) this.graceTiles -= 1;
        else if (this.rng.chance(ENCOUNTER_CHANCE_PER_TILE)) this.encounter();
      }
    }
  }

  private idle() {
    this.player.anims.stop();
    this.player.setFrame(PLAYER_ROW[this.facing] * PLAYER_FRAMES_PER_ROW);
  }

  private createAnimations() {
    for (const facing of Object.keys(PLAYER_ROW) as Facing[]) {
      const key = `walk-${facing}`;
      if (this.anims.exists(key)) continue;
      const row = PLAYER_ROW[facing] * PLAYER_FRAMES_PER_ROW;
      this.anims.create({
        key,
        frames: [1, 0, 2, 0].map((f) => ({ key: "player", frame: row + f })),
        frameRate: 8,
        repeat: -1,
      });
    }
  }

  // ---------------------------------------------------------------- shared state

  private onWorld() {
    const s = worldStore.getSnapshot().summary;
    this.darkTarget = s ? darknessFor(s.hour) : MAX_DARK;
  }

  /** Freeze the player and give the keyboard back to the page while a panel or dialog is open. */
  private syncInputCapture() {
    const kb = this.input.keyboard;
    if (!kb) return;
    this.blocked = isBlocking(uiStore.getSnapshot());
    const el = document.activeElement as HTMLElement | null;
    const typing = !!el && el !== document.body && el.matches("button, input, select, textarea, a[href], [tabindex]:not(canvas)");
    kb.enabled = !this.blocked;
    if (this.blocked || typing) kb.disableGlobalCapture();
    else kb.enableGlobalCapture();
    if (this.blocked) kb.resetKeys();
  }

  // ---------------------------------------------------------------- encounters

  private encounter() {
    const ward = worldStore.getSnapshot().summary?.wards.find((w) => w.id === "lantern");
    const grudge = ward?.grudge ?? { rage: 0, sorrow: 5, dread: 0 };
    const total = ward?.total ?? 5;
    const omen = rollSpawnOmen(total, this.rng);
    const emotion = rollSpawnEmotion(grudge, this.rng);
    const curse = this.rng.pick(cursesWhere(emotion, omen));
    if (!this.reduceMotion) {
      this.cameras.main.shake(160, 0.003);
      this.cameras.main.flash(180, 176, 21, 58);
    }
    uiStore.openEncounter(curse);
  }

  /** The player pressed "Fight!": pause the Ward and run the battle on top of it. */
  private maybeStartBattle() {
    const req = uiStore.getSnapshot().battle;
    if (!req || this.fighting) return;
    this.fighting = true;
    const partner = playerStore.partner();
    const data: BattleData = { wild: req.wild, omen: req.omen, seed: req.seed, playerCurse: partner.speciesId };
    this.game.events.once(BATTLE_END, (r: BattleResult) => void this.afterBattle(req, r));
    this.scene.pause();
    this.scene.launch("battle", data);
  }

  /** Back in the Ward: report the result to the town, then pay out points and talismans. */
  private async afterBattle(req: BattleRequest, r: BattleResult) {
    this.fighting = false;
    this.grassDistance = 0;
    this.graceTiles = GRACE_TILES;
    uiStore.endBattle();
    this.scene.resume();

    const sp = curseById(r.wild);
    const partnerName = curseById(r.playerCurse).name;
    const summary = worldStore.getSnapshot().summary;
    const day = summary?.day ?? 1;
    const verb = captureVerb(r.outcome);
    const report = { outcome: r.outcome, wild: r.wild, omen: req.omen, seed: r.seed, day, points: 0 };

    if (!verb) {
      playerStore.recordBattle(report);
      worldStore.say("battle", r.outcome === "fled"
        ? `You slipped away from ${sp.name}. It still haunts the tall grass.`
        : `${sp.name} drove you off. I patched up your ${partnerName}; rest a moment and try again.`);
      return;
    }

    const proof = { wild: r.wild, omen: req.omen, playerCurse: r.playerCurse, seed: r.seed, actions: r.actions };
    const answer = await worldStore.reportCapture({ ward: "lantern", speciesId: r.wild, omen: req.omen, verb, by: PLAYER_NAME }, proof);
    if (answer.points === null) {
      playerStore.recordBattle({ ...report, outcome: "fled" });
      worldStore.say("rule-rejected", `The Rite could not record that fight (${answer.error}). No points this time.`);
      return;
    }
    const points = answer.points;
    const bonus = points > POINTS_FOR_OMEN[req.omen] ? " A rule in force raised your reward." : "";
    playerStore.recordBattle({ ...report, points });
    worldStore.say(r.outcome === "bound" ? "bound" : "exorcised", r.outcome === "bound"
      ? `${sp.name} is bound. Its talisman is in your case (T). +${points} points.${bonus}`
      : `${sp.name} is exorcised and the Ward breathes easier. +${points} points.${bonus}`);
  }

  // ---------------------------------------------------------------- dressing

  private placeProps() {
    // Kessa floats by the Rite board; click her to rewrite the rules.
    const kessa = this.add.image(BOARD.x + 30, BOARD.y - 6, "kessa").setDepth(5);
    kessa.setInteractive({ useHandCursor: true }).on("pointerdown", () => {
      if (!this.blocked) uiStore.openPanel("rules");
    });
    if (!this.reduceMotion) {
      this.tweens.add({ targets: kessa, y: kessa.y - 4, duration: 1400, yoyo: true, repeat: -1, ease: "Sine.InOut" });
    }
    this.lamps.push({ x: kessa.x, y: kessa.y, r: 40, seed: 3 });

    // Torn receipts caught in the tall grass: something lives in there.
    this.layer.forEachTile((t) => {
      if (t.index !== T.tallGrass || ((t.x * 73 + t.y * 151) % 11) !== 0) return;
      this.add.image(t.pixelX + 8 + ((t.x * 13) % 16), t.pixelY + 6 + ((t.y * 7) % 14), (t.x + t.y) % 2 ? "scrap-red" : "scrap")
        .setAngle(((t.x * 37) % 50) - 25).setDepth(1);
    });

    // Porters in straw hats loitering by the stalls.
    const porters: [number, number][] = [[9.5, 13.6], [34.5, 13.2], [27.8, 16.4]];
    for (const [tx, ty] of porters) {
      const p = this.physics.add.staticImage(tx * TILE, ty * TILE, "porter").setDepth(3);
      p.refreshBody();
      this.physics.add.collider(this.player, p);
      this.add.image(p.x, p.y + 8, "shadow").setDepth(2).setScale(0.8);
      if (!this.reduceMotion) this.tweens.add({ targets: p, y: p.y - 1, duration: 900 + tx * 20, yoyo: true, repeat: -1, ease: "Stepped" });
    }
  }

  /** Strings of paper lanterns and talisman flags zig-zagging over the market street. */
  private buildLanternStrings() {
    const north: Phaser.Math.Vector2[] = [];
    const south: Phaser.Math.Vector2[] = [];
    this.layer.forEachTile((t) => {
      if (t.index !== T.lantern) return;
      const head = new Phaser.Math.Vector2(t.pixelX + 16, t.pixelY + 4);
      (t.y < 13 ? north : south).push(head);
      this.lamps.push({ x: t.pixelX + 16, y: t.pixelY + 10, r: 76, seed: t.x * 7 + t.y });
    });
    north.sort((a, b) => a.x - b.x);
    south.sort((a, b) => a.x - b.x);

    const g = this.add.graphics().setDepth(5);
    const string = (a: Phaser.Math.Vector2, b: Phaser.Math.Vector2, sag: number, items: number) => {
      const mid = new Phaser.Math.Vector2((a.x + b.x) / 2, (a.y + b.y) / 2 + sag);
      const curve = new Phaser.Curves.QuadraticBezier(a, mid, b);
      g.lineStyle(1, 0x3a2a20, 1);
      curve.draw(g, 24);
      for (let i = 0; i < items; i++) {
        const p = curve.getPoint((i + 0.5) / items);
        const lantern = i % 2 === 0;
        const item = this.add.image(Math.round(p.x), Math.round(p.y), lantern ? "mini-lantern" : "flag").setOrigin(0.5, 0).setDepth(5);
        if (lantern) this.lamps.push({ x: p.x, y: p.y + 5, r: 26, seed: p.x });
        if (!this.reduceMotion) {
          item.setAngle(-5);
          this.tweens.add({ targets: item, angle: 5, duration: 1300 + (i % 3) * 250, yoyo: true, repeat: -1, ease: "Sine.InOut", delay: i * 90 });
        }
      }
    };
    north.forEach((a, i) => {
      const b = i % 2 === 0 ? south[i + 1] : south[i - 1];
      if (b && Math.abs(a.x - b.x) < TILE * 6) string(a, b, 14, 7);
      const next = north[i + 1];
      if (next && next.x - a.x <= TILE * 4) string(a, next, 10, 5);
    });
    south.forEach((a, i) => {
      const next = south[i + 1];
      if (next && next.x - a.x <= TILE * 4) string(a, next, 10, 5);
    });
    // across the plaza, between the two banner posts
    const left = new Phaser.Math.Vector2(18 * TILE + 16, 9 * TILE + 2);
    const right = new Phaser.Math.Vector2(24 * TILE + 16, 9 * TILE + 2);
    string(left, right, 18, 9);
  }

  private buildLighting() {
    const W = this.scale.width, H = this.scale.height;
    // Screen-fixed objects are still scaled by camera zoom around the centre, so size them to the zoomed view.
    const w = W / ZOOM, h = H / ZOOM;
    this.fx = { x: (W - w) / 2, y: (H - h) / 2, w, h };

    for (const l of this.lamps) {
      l.glow = this.add.image(l.x, l.y, "glow").setBlendMode(Phaser.BlendModes.ADD).setDepth(6).setScale((l.r * 1.6) / 64);
    }

    this.nightRt = this.add.renderTexture(this.fx.x, this.fx.y, w, h).setOrigin(0).setScrollFactor(0).setDepth(8);
    this.brush = this.make.image({ key: "light", add: false });
    this.vignette = this.add.image(this.fx.x, this.fx.y, "vignette").setOrigin(0).setDisplaySize(w, h).setScrollFactor(0).setDepth(9);

    // Redraw after the camera has followed the player this frame, so lights never lag behind.
    this.cameras.main.on(Phaser.Cameras.Scene2D.Events.FOLLOW_UPDATE, () => this.drawNight());
  }

  private drawNight() {
    const d = this.dark;
    if (d < 0.02) {
      this.nightRt.setVisible(false);
      return;
    }
    const t = Phaser.Math.Clamp(d / MAX_DARK, 0, 1);
    const c = Phaser.Display.Color.Interpolate.ColorWithColor(DUSK_COLOR, NIGHT_COLOR, 100, Math.round(t * 100));
    // Fill opaque and fade the whole layer: a partly transparent fill renders far too faint in WebGL.
    this.nightRt.setVisible(true).setAlpha(d).clear().fill(Phaser.Display.Color.GetColor(c.r, c.g, c.b), 1);
    const view = this.cameras.main.worldView;
    const now = this.time.now;
    for (const l of this.lamps) {
      const sx = l.x - view.x, sy = l.y - view.y;
      if (sx < -l.r || sy < -l.r || sx > this.fx.w + l.r || sy > this.fx.h + l.r) continue;
      const flicker = this.reduceMotion ? 1 : 1 + Math.sin(now * 0.004 + l.seed) * 0.05;
      this.brush.setScale((l.r * 2 * flicker) / 128);
      this.nightRt.erase(this.brush, sx, sy);
    }
  }

  /** Drifting paper scraps over the whole view, and leaves kicked up from tall grass. */
  private buildAmbience() {
    this.leafFx = this.add.particles(0, 0, "leaf", {
      emitting: false,
      lifespan: 450,
      speed: { min: 20, max: 55 },
      angle: { min: 200, max: 340 },
      gravityY: 140,
      alpha: { start: 1, end: 0 },
    }).setDepth(4);

    const count = this.reduceMotion ? 0 : 1;
    if (!count) return;
    const { x, y, w, h } = this.fx;
    const drift = {
      x: { min: -20, max: w },
      y: { min: -10, max: h },
      lifespan: 9000,
      speedX: { min: 6, max: 20 },
      speedY: { min: 4, max: 12 },
      rotate: { start: 0, end: 300 },
      alpha: { start: 0.9, end: 0 },
    };
    this.add.particles(x, y, "scrap", { ...drift, frequency: 900 }).setScrollFactor(0).setDepth(7);
    this.add.particles(x, y, "scrap-red", { ...drift, frequency: 2600 }).setScrollFactor(0).setDepth(7);
  }

  private animateWater() {
    if (this.reduceMotion) return;
    let flip = false;
    this.time.addEvent({
      delay: 650,
      loop: true,
      callback: () => {
        flip = !flip;
        if (flip) this.layer.replaceByIndex(T.water, T.water2);
        else this.layer.replaceByIndex(T.water2, T.water);
      },
    });
  }
}

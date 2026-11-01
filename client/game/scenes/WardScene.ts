import * as Phaser from "phaser";
import {
  EMOTION_INFO, EMOTIONS, cursesWhere, rollSpawnEmotion, rollSpawnOmen, createRng,
  type CurseSpecies, type GameEvent, type WorldSummary,
} from "@grudgebound/shared";
import { SOLID, T, TILE, buildLanternWard } from "../tiles";
import { startWorldFeed, type WorldFeed } from "../worldFeed";

const SPEED = 140;
const ENCOUNTER_CHANCE_PER_TILE = 0.12;
const FONT = { fontFamily: "monospace", fontSize: "15px", color: "#F3E9D2" };
const PANEL = 0x1a1726;

/** Lantern Ward: walk around, step into tall grass, meet curses. Battles arrive Days 3-6. */
export class WardScene extends Phaser.Scene {
  private player!: Phaser.Physics.Arcade.Sprite;
  private cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
  private wasd!: Record<"W" | "A" | "S" | "D", Phaser.Input.Keyboard.Key>;
  private layer!: Phaser.Tilemaps.TilemapLayer;
  private hudText!: Phaser.GameObjects.Text;
  private grudgeBar!: Phaser.GameObjects.Graphics;
  private banner!: Phaser.GameObjects.Container;
  private bannerText!: Phaser.GameObjects.Text;
  private dialog!: Phaser.GameObjects.Container;
  private dialogText!: Phaser.GameObjects.Text;
  private dialogCurse!: Phaser.GameObjects.Graphics;
  private inDialog = false;
  private grassDistance = 0;
  private feed?: WorldFeed;
  private rng = createRng(Date.now() >>> 0);

  constructor() {
    super("ward");
  }

  create() {
    const data = buildLanternWard();
    const map = this.make.tilemap({ data, tileWidth: TILE, tileHeight: TILE });
    const tileset = map.addTilesetImage("tiles", "tiles", TILE, TILE, 0, 0)!;
    this.layer = map.createLayer(0, tileset, 0, 0)!;
    this.layer.setCollision(SOLID);

    this.player = this.physics.add.sprite(21.5 * TILE, 13.5 * TILE, "player");
    this.player.setSize(16, 12).setOffset(4, 18);
    this.physics.add.collider(this.player, this.layer);
    this.physics.world.setBounds(0, 0, map.widthInPixels, map.heightInPixels);
    this.player.setCollideWorldBounds(true);

    this.cameras.main.setBounds(0, 0, map.widthInPixels, map.heightInPixels);
    this.cameras.main.startFollow(this.player, true, 0.15, 0.15);

    const kb = this.input.keyboard!;
    this.cursors = kb.createCursorKeys();
    this.wasd = kb.addKeys("W,A,S,D") as Record<"W" | "A" | "S" | "D", Phaser.Input.Keyboard.Key>;
    kb.on("keydown-SPACE", () => this.closeDialog());
    kb.on("keydown-ENTER", () => this.closeDialog());

    this.buildHud();
    this.buildBanner();
    this.buildDialog();

    let destroyed = false;
    this.events.once(Phaser.Scenes.Events.DESTROY, () => {
      destroyed = true;
      this.feed?.stop();
    });
    void startWorldFeed(
      (s) => { if (!destroyed) this.updateHud(s); },
      (e) => { if (!destroyed) this.onWorldEvent(e); },
    ).then((f) => {
      if (destroyed) return f.stop();
      this.feed = f;
      this.showBanner(f.mode === "server"
        ? "Kessa: \"Welcome to the Rite. Connected to the town.\""
        : "Kessa: \"Welcome to the Rite.\" (offline: the town runs in your browser)");
    });
  }

  update() {
    const body = this.player.body as Phaser.Physics.Arcade.Body;
    if (this.inDialog) {
      body.setVelocity(0, 0);
      return;
    }
    const left = this.cursors.left.isDown || this.wasd.A.isDown;
    const right = this.cursors.right.isDown || this.wasd.D.isDown;
    const up = this.cursors.up.isDown || this.wasd.W.isDown;
    const down = this.cursors.down.isDown || this.wasd.S.isDown;
    const vx = (right ? 1 : 0) - (left ? 1 : 0);
    const vy = (down ? 1 : 0) - (up ? 1 : 0);
    const len = Math.hypot(vx, vy) || 1;
    body.setVelocity((vx / len) * SPEED, (vy / len) * SPEED);
    if (vx !== 0) this.player.setFlipX(vx < 0);

    // Curses lurk in tall grass: roll once per tile walked.
    const tile = this.layer.getTileAtWorldXY(this.player.x, this.player.y + 10);
    if (tile?.index === T.tallGrass && (vx || vy)) {
      this.grassDistance += (SPEED * this.game.loop.delta) / 1000;
      if (this.grassDistance >= TILE) {
        this.grassDistance = 0;
        if (this.rng.chance(ENCOUNTER_CHANCE_PER_TILE)) this.encounter();
      }
    }
  }

  // ---------------------------------------------------------------- encounters

  private encounter() {
    const ward = this.feed?.latest.wards.find((w) => w.id === "lantern");
    const grudge = ward?.grudge ?? { rage: 0, sorrow: 5, dread: 0 };
    const total = ward?.total ?? 5;
    const omen = rollSpawnOmen(total, this.rng);
    const emotion = rollSpawnEmotion(grudge, this.rng);
    const curse = this.rng.pick(cursesWhere(emotion, omen));
    this.openDialog(curse);
  }

  private openDialog(c: CurseSpecies) {
    this.inDialog = true;
    const info = EMOTION_INFO[c.emotion];
    this.dialogText.setText(
      `A wild ${c.name} appears!\n` +
      `${info.label} · ${c.omen[0]!.toUpperCase()}${c.omen.slice(1)}   (born from ${c.bornFrom})\n` +
      `Signature move: ${c.signatureMove.name}, ${c.signatureMove.effect}.\n\n` +
      `Battles and binding arrive on Days 3-6.  [SPACE] to continue`,
    );
    // placeholder curse portrait in its emotion color
    const g = this.dialogCurse;
    g.clear();
    const color = Phaser.Display.Color.HexStringToColor(info.color).color;
    const size = c.omen === "calamity" ? 34 : c.omen === "wail" ? 28 : 22;
    g.fillStyle(0xf3e9d2, 1).fillRoundedRect(-44, -44, 88, 88, 6);
    g.fillStyle(color, 1).fillCircle(0, 4, size);
    g.fillStyle(0xf3e9d2, 1).fillCircle(-9, 0, 6).fillCircle(9, 0, 6);
    g.fillStyle(0x1a1726, 1).fillCircle(-9, 1, 3).fillCircle(9, 1, 3);
    this.dialog.setVisible(true);
  }

  private closeDialog() {
    if (!this.inDialog) return;
    this.inDialog = false;
    this.dialog.setVisible(false);
  }

  // ---------------------------------------------------------------- HUD

  private buildHud() {
    const bg = this.add.graphics().setScrollFactor(0).setDepth(10);
    bg.fillStyle(PANEL, 0.85).fillRoundedRect(10, 10, 300, 74, 6);
    this.hudText = this.add.text(22, 18, "Lantern Ward", FONT).setScrollFactor(0).setDepth(11);
    this.grudgeBar = this.add.graphics().setScrollFactor(0).setDepth(11);
    this.add.text(this.scale.width - 12, 12, "Arrows / WASD to walk · tall grass hides curses", { ...FONT, fontSize: "12px" })
      .setOrigin(1, 0).setScrollFactor(0).setDepth(11).setAlpha(0.8);
  }

  private updateHud(s: WorldSummary) {
    const w = s.wards.find((x) => x.id === "lantern");
    if (!w) return;
    const hh = String(s.hour).padStart(2, "0");
    this.hudText.setText(`Lantern Ward   Day ${s.day} ${hh}:00${s.night ? " (night)" : ""}\nGrudge ${Math.round(w.total)}/100   Rules in force: ${s.rules.length}`);
    const g = this.grudgeBar;
    g.clear();
    const x0 = 22, y0 = 62, width = 276, height = 10;
    g.fillStyle(0x3a3547, 1).fillRect(x0, y0, width, height);
    let x = x0;
    for (const e of EMOTIONS) {
      const segment = (w.grudge[e] / 100) * width;
      g.fillStyle(Phaser.Display.Color.HexStringToColor(EMOTION_INFO[e].color).color, 1).fillRect(x, y0, segment, height);
      x += segment;
    }
  }

  // ---------------------------------------------------------------- Kessa's banner

  private buildBanner() {
    const width = 640;
    const bg = this.add.graphics();
    bg.fillStyle(0xf3e9d2, 0.96).fillRoundedRect(-width / 2, -22, width, 44, 8);
    bg.lineStyle(2, 0xb0153a, 1).strokeRoundedRect(-width / 2, -22, width, 44, 8);
    this.bannerText = this.add.text(0, 0, "", { ...FONT, color: "#1A1726", fontSize: "14px", align: "center", wordWrap: { width: width - 30 } }).setOrigin(0.5);
    this.banner = this.add.container(this.scale.width / 2, this.scale.height - 40, [bg, this.bannerText])
      .setScrollFactor(0).setDepth(20).setVisible(false);
  }

  private showBanner(text: string) {
    this.bannerText.setText(text);
    this.banner.setVisible(true).setAlpha(1);
    this.tweens.killTweensOf(this.banner);
    this.tweens.add({ targets: this.banner, alpha: 0, delay: 4500, duration: 600, onComplete: () => this.banner.setVisible(false) });
  }

  private onWorldEvent(e: GameEvent) {
    if (["rule-added", "rule-rejected", "bankruptcy", "calamity", "reopen"].includes(e.type)) this.showBanner(e.text);
  }

  // ---------------------------------------------------------------- dialog

  private buildDialog() {
    const w = 760, h = 150;
    const bg = this.add.graphics();
    bg.fillStyle(PANEL, 0.95).fillRoundedRect(-w / 2, -h / 2, w, h, 8);
    bg.lineStyle(2, 0xd9a532, 1).strokeRoundedRect(-w / 2, -h / 2, w, h, 8);
    this.dialogCurse = this.add.graphics({ x: -w / 2 + 70, y: 0 });
    this.dialogText = this.add.text(-w / 2 + 135, -h / 2 + 18, "", { ...FONT, lineSpacing: 6, wordWrap: { width: w - 160 } });
    this.dialog = this.add.container(this.scale.width / 2, this.scale.height - 110, [bg, this.dialogCurse, this.dialogText])
      .setScrollFactor(0).setDepth(30).setVisible(false);
  }
}

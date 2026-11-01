import Phaser from "phaser";
import { BootScene } from "./scenes/BootScene";
import { WardScene } from "./scenes/WardScene";

const game = new Phaser.Game({
  type: Phaser.AUTO,
  parent: "game",
  width: 960,
  height: 540,
  backgroundColor: "#1a1726",
  pixelArt: true,
  roundPixels: true,
  physics: { default: "arcade", arcade: { debug: false } },
  scale: { mode: Phaser.Scale.FIT, autoCenter: Phaser.Scale.CENTER_BOTH },
  scene: [BootScene, WardScene],
});

// Dev only: inspect the game from the browser console, e.g. game.scene.getScene("ward").
if (import.meta.env.DEV) (window as unknown as { game: Phaser.Game }).game = game;

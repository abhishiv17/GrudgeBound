/**
 * Creates the Phaser game. Only ever imported in the browser (see components/GameCanvas.tsx),
 * because Phaser needs `window` and must not run during Next.js server rendering.
 */
import * as Phaser from "phaser";
import { BootScene } from "./scenes/BootScene";
import { WardScene } from "./scenes/WardScene";

export function startGame(parent: HTMLElement): Phaser.Game {
  const game = new Phaser.Game({
    type: Phaser.AUTO,
    parent,
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
  if (process.env.NODE_ENV !== "production") (window as unknown as { game: Phaser.Game }).game = game;
  return game;
}

import Phaser from "phaser";
import { TILE, TILE_COUNT, drawTiles } from "../tiles";

/** Builds placeholder textures in code so the game runs with zero asset files. */
export class BootScene extends Phaser.Scene {
  constructor() {
    super("boot");
  }

  create() {
    const tiles = this.textures.createCanvas("tiles", TILE * TILE_COUNT, TILE);
    if (tiles) {
      drawTiles(tiles.getContext());
      tiles.refresh();
    }

    // Player: young exorcist with a red scarf (placeholder until the sprite sheet arrives)
    const p = this.textures.createCanvas("player", 24, 30);
    if (p) {
      const c = p.getContext();
      c.fillStyle = "#1A1726"; c.fillRect(6, 0, 12, 7); c.fillRect(9, -1, 6, 3);   // hair + bun
      c.fillStyle = "#F2C9A0"; c.fillRect(7, 5, 10, 8);                              // face
      c.fillStyle = "#1A1726"; c.fillRect(9, 8, 2, 2); c.fillRect(13, 8, 2, 2);      // eyes
      c.fillStyle = "#B0153A"; c.fillRect(4, 13, 16, 4); c.fillRect(17, 15, 6, 3);   // scarf
      c.fillStyle = "#2B3A55"; c.fillRect(6, 17, 12, 8);                             // tunic
      c.fillStyle = "#F3E9D2"; c.fillRect(17, 18, 4, 5);                             // talisman
      c.fillStyle = "#1A1726"; c.fillRect(7, 25, 4, 5); c.fillRect(13, 25, 4, 5);    // legs
      p.refresh();
    }

    this.scene.start("ward");
  }
}

import * as Phaser from "phaser";
import { TILE, TILE_COUNT, drawTiles } from "../tiles";
import { buildSpriteTextures } from "../sprites";

/** Builds every texture in code so the game runs with zero asset files. */
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
    buildSpriteTextures(this);
    this.scene.start("ward");
  }
}

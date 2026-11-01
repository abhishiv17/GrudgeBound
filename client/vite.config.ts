import { defineConfig } from "vite";

export default defineConfig({
  server: { port: 5173 },
  build: {
    // Phaser alone is ~1.2 MB minified; that's expected for a game.
    chunkSizeWarningLimit: 2000,
  },
});

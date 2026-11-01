"use client";

import { useEffect, useRef } from "react";
import type * as Phaser from "phaser";

/** Mounts the Phaser game in the browser only, and destroys it when the page unmounts. */
export default function GameCanvas() {
  const host = useRef<HTMLDivElement>(null);

  useEffect(() => {
    let game: Phaser.Game | undefined;
    let cancelled = false;
    void import("../game/start").then(({ startGame }) => {
      if (!cancelled && host.current) game = startGame(host.current);
    });
    return () => {
      cancelled = true;
      game?.destroy(true);
    };
  }, []);

  return <div ref={host} className="game" />;
}

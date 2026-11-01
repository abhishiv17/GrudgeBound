"use client";

import { useEffect } from "react";
import { touchInput } from "../../lib/uiStore";

type Dir = keyof typeof touchInput;
const DIRS: { dir: Dir; label: string; glyph: string }[] = [
  { dir: "up", label: "Walk up", glyph: "▲" },
  { dir: "left", label: "Walk left", glyph: "◀" },
  { dir: "right", label: "Walk right", glyph: "▶" },
  { dir: "down", label: "Walk down", glyph: "▼" },
];

/** On-screen D-pad for touch screens (hidden on devices with a fine pointer). */
export default function TouchPad({ className }: { className?: string }) {
  useEffect(() => () => {
    touchInput.up = touchInput.down = touchInput.left = touchInput.right = false;
  }, []);

  const press = (dir: Dir, on: boolean) => (e: React.PointerEvent) => {
    e.preventDefault();
    touchInput[dir] = on;
  };

  return (
    <div className={`touchpad ${className ?? ""}`} aria-hidden>
      {DIRS.map(({ dir, label, glyph }) => (
        <button
          key={dir}
          type="button"
          tabIndex={-1}
          aria-label={label}
          className={`touchpad__btn touchpad__btn--${dir}`}
          onPointerDown={press(dir, true)}
          onPointerUp={press(dir, false)}
          onPointerCancel={press(dir, false)}
          onPointerLeave={press(dir, false)}
          onContextMenu={(e) => e.preventDefault()}
        >
          {glyph}
        </button>
      ))}
    </div>
  );
}

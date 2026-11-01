import { useMemo, type CSSProperties } from "react";
import { rasterize, type Grid, type PixelMap } from "../../game/pixelArt";

interface Props {
  /** A pixel map, or a ready grid of colors. */
  map?: PixelMap;
  grid?: Grid;
  /** Screen pixels per art pixel. */
  scale?: number;
  overrides?: Record<string, string>;
  /** Accessible name; without it the sprite is decorative. */
  title?: string;
  className?: string;
  style?: CSSProperties;
  flip?: boolean;
}

/** Renders pixel art as crisp inline SVG (same art as the Phaser textures). */
export default function PixelSprite({ map, grid, scale = 2, overrides, title, className, style, flip }: Props) {
  const cells = useMemo(() => grid ?? (map ? rasterize(map, overrides) : []), [grid, map, overrides]);
  const h = cells.length;
  const w = h ? cells[0]!.length : 0;

  // Merge horizontal runs of one color into a single rect.
  const rects = useMemo(() => {
    const out: { x: number; y: number; w: number; c: string }[] = [];
    cells.forEach((row, y) => {
      let x = 0;
      while (x < row.length) {
        const c = row[x];
        if (!c) { x++; continue; }
        let run = 1;
        while (row[x + run] === c) run++;
        out.push({ x, y, w: run, c });
        x += run;
      }
    });
    return out;
  }, [cells]);

  return (
    <svg
      className={className}
      width={w * scale}
      height={h * scale}
      viewBox={`0 0 ${w} ${h}`}
      shapeRendering="crispEdges"
      style={{ ...(flip ? { transform: "scaleX(-1)" } : null), ...style }}
      role={title ? "img" : undefined}
      aria-hidden={title ? undefined : true}
      focusable="false"
    >
      {title ? <title>{title}</title> : null}
      {rects.map((r, i) => (
        <rect key={i} x={r.x} y={r.y} width={r.w} height={1} fill={r.c} />
      ))}
    </svg>
  );
}

import PixelSprite from "./ui/PixelSprite";
import { KESSA, KESSA_STERN } from "../game/pixelArt";

interface Props {
  /** Screen pixels per art pixel. */
  scale?: number;
  mood?: "happy" | "stern";
  /** Floating paper strips and a soft glow (title screen and verdicts). */
  strips?: boolean;
  className?: string;
  title?: string;
}

/** Kessa, the paper-lantern referee: pixel lantern body with talisman strips drifting beside her. */
export default function Kessa({ scale = 4, mood = "happy", strips = true, className, title }: Props) {
  return (
    <span className={`kessa${strips ? " kessa--strips" : ""}${className ? ` ${className}` : ""}`} style={{ ["--kessa-scale" as string]: scale }}>
      {strips ? (
        <>
          <span className="kessa__glow" aria-hidden />
          <span className="kessa__strip kessa__strip--l" aria-hidden />
          <span className="kessa__strip kessa__strip--r" aria-hidden />
        </>
      ) : null}
      <PixelSprite map={mood === "stern" ? KESSA_STERN : KESSA} scale={scale} title={title} className="kessa__body" />
    </span>
  );
}

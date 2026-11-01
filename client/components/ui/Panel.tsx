import type { ElementType, ReactNode } from "react";
import PixelSprite from "./PixelSprite";
import { SEAL } from "../../game/pixelArt";

interface Props {
  children: ReactNode;
  /** Heading shown on the panel's title strip. */
  title?: ReactNode;
  titleId?: string;
  /** Hang a wax seal from the top edge. */
  seal?: boolean;
  /** "paper" (default) or "dark" (ink panel with gold trim). */
  tone?: "paper" | "dark";
  className?: string;
  as?: ElementType;
}

/**
 * Parchment panel: a notched pixel frame (stacked box-shadows), a ruled inner border,
 * crimson diamond corner pins and an optional wax seal. Pure CSS, no images.
 */
export default function Panel({ children, title, titleId, seal, tone = "paper", className, as: Tag = "section" }: Props) {
  return (
    <Tag className={`panel panel--${tone}${className ? ` ${className}` : ""}`} aria-labelledby={title && titleId ? titleId : undefined}>
      <span className="panel__corner panel__corner--tl" aria-hidden />
      <span className="panel__corner panel__corner--tr" aria-hidden />
      <span className="panel__corner panel__corner--bl" aria-hidden />
      <span className="panel__corner panel__corner--br" aria-hidden />
      {seal ? <PixelSprite map={SEAL} scale={3} className="panel__seal" /> : null}
      {title ? (
        <h2 className="panel__title" id={titleId}>
          {title}
        </h2>
      ) : null}
      {children}
    </Tag>
  );
}

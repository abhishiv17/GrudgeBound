import type { ReactNode } from "react";

/** An inked rubber stamp: rotated, double-ruled, slightly worn. */
export default function Stamp({ children, tone = "crimson", className }: { children: ReactNode; tone?: "crimson" | "ink" | "gold"; className?: string }) {
  return <span className={`stamp stamp--${tone}${className ? ` ${className}` : ""}`}>{children}</span>;
}

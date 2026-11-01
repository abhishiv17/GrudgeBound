import type { Metadata, Viewport } from "next";
import { Courier_Prime, Pixelify_Sans } from "next/font/google";
import "./globals.css";
import "./styles/ui.css";
import "./styles/title.css";
import "./styles/hud.css";
import "./styles/panels.css";

/** Pixel display face for titles, numbers and stamps. */
const display = Pixelify_Sans({ subsets: ["latin"], variable: "--font-display", display: "swap" });
/** Typewriter body face: ledgers, receipts and Kessa's notices. */
const body = Courier_Prime({ subsets: ["latin"], weight: ["400", "700"], variable: "--font-body", display: "swap" });

export const metadata: Metadata = {
  title: "Grudgebound: The Rite",
  description: "A creature-binding game where greed creates monsters and players rewrite the rules. Built at NEAR HACKATHON.",
};

export const viewport: Viewport = {
  themeColor: "#1A1726",
  width: "device-width",
  initialScale: 1,
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className={`${display.variable} ${body.variable}`}>
      <body>{children}</body>
    </html>
  );
}

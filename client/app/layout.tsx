import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Grudgebound: The Rite",
  description: "A creature-binding game where greed creates monsters and players rewrite the rules. Built at NEAR HACKATHON.",
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}

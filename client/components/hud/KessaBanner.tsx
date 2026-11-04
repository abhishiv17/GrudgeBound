"use client";

import { useEffect, useState } from "react";
import Kessa from "../Kessa";
import { useReducedMotion, useWorld } from "../../lib/hooks";
import type { Announcement } from "../../lib/worldStore";

const LABEL: Record<string, string> = {
  welcome: "Welcome",
  "rule-added": "A new rule",
  "rule-rejected": "Rule refused",
  bankruptcy: "Bankruptcy",
  calamity: "Calamity",
  reopen: "Shop reopened",
  scam: "Forged talisman",
  bound: "Bound",
  exorcised: "Exorcised",
  battle: "After the fight",
};

const HOLD_MS = 5200;
const CHARS_PER_TICK = 2;
const TICK_MS = 28;

/** Kessa's announcements on an unrolled paper scroll, typed out letter by letter. */
export default function KessaBanner({ className }: { className?: string }) {
  const { announcements } = useWorld();
  const reduced = useReducedMotion();
  const [lastId, setLastId] = useState(0);
  const [current, setCurrent] = useState<Announcement | null>(null);
  const [typed, setTyped] = useState(0);
  const [open, setOpen] = useState(false);

  // Take the next announcement when the scroll is free (skip ahead if many piled up).
  useEffect(() => {
    if (current) return;
    const pending = announcements.filter((a) => a.id > lastId);
    if (!pending.length) return;
    const next = pending.length > 2 ? pending[pending.length - 2]! : pending[0]!;
    setCurrent(next);
    setLastId(next.id);
    setTyped(0);
    setOpen(true);
  }, [announcements, current, lastId]);

  const length = current?.text.length ?? 0;
  const done = typed >= length;

  // Typewriter reveal (instant with reduced motion).
  useEffect(() => {
    if (!current || done) return;
    if (reduced) {
      setTyped(length);
      return;
    }
    const t = setTimeout(() => setTyped((n) => Math.min(length, n + CHARS_PER_TICK)), TICK_MS);
    return () => clearTimeout(t);
  }, [current, typed, done, length, reduced]);

  // Hold, then roll the scroll up.
  useEffect(() => {
    if (!current || !done || !open) return;
    const t = setTimeout(() => setOpen(false), HOLD_MS);
    return () => clearTimeout(t);
  }, [current, done, open]);

  // Free the scroll once it has rolled up.
  useEffect(() => {
    if (open || !current) return;
    const t = setTimeout(() => setCurrent(null), reduced ? 0 : 320);
    return () => clearTimeout(t);
  }, [open, current, reduced]);

  const skip = () => {
    if (!done) setTyped(length);
    else setOpen(false);
  };

  const isAlarm = current?.kind === "calamity" || current?.kind === "bankruptcy" || current?.kind === "rule-rejected" || current?.kind === "scam";

  return (
    <div className={`kessa-scroll ${open && current ? "is-open" : ""} ${isAlarm ? "kessa-scroll--alarm" : ""} ${className ?? ""}`}>
      <div role="status" aria-live="polite" className="sr-only">
        {current ? `Kessa: ${current.text}` : ""}
      </div>
      {current ? (
        <div className="kessa-scroll__inner" aria-hidden>
          <span className="kessa-scroll__roll kessa-scroll__roll--l" />
          <div className="kessa-scroll__paper">
            <Kessa scale={2} strips={false} mood={current.kind === "rule-rejected" ? "stern" : "happy"} className="kessa-scroll__icon" />
            <div className="kessa-scroll__text">
              <span className="kessa-scroll__speaker">
                Kessa <span aria-hidden>·</span> {LABEL[current.kind] ?? "Notice"}
              </span>
              <p>
                {current.text.slice(0, typed)}
                {!done ? <span className="caret" /> : null}
              </p>
            </div>
            <button type="button" className="kessa-scroll__skip" onClick={skip} tabIndex={-1}>
              {done ? "×" : "»"}
            </button>
          </div>
          <span className="kessa-scroll__roll kessa-scroll__roll--r" />
        </div>
      ) : null}
    </div>
  );
}

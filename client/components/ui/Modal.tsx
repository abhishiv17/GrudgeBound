"use client";

import { useEffect, useId, useRef, type ReactNode } from "react";
import PixelSprite from "./PixelSprite";
import { SEAL } from "../../game/pixelArt";

interface Props {
  open: boolean;
  onClose: () => void;
  title: ReactNode;
  /** Short line under the title. */
  subtitle?: ReactNode;
  children: ReactNode;
  className?: string;
}

/**
 * A parchment dialog built on the native <dialog> element: focus is trapped while open,
 * Esc closes it, focus returns to the opener afterwards, and the page behind is inert.
 */
export default function Modal({ open, onClose, title, subtitle, children, className }: Props) {
  const ref = useRef<HTMLDialogElement>(null);
  const titleId = useId();
  const onCloseRef = useRef(onClose);
  useEffect(() => {
    onCloseRef.current = onClose;
  });

  useEffect(() => {
    const d = ref.current;
    if (!d) return;
    if (open && !d.open) d.showModal();
    if (!open && d.open) d.close();
  }, [open]);

  useEffect(() => {
    const d = ref.current;
    if (!d) return;
    const handle = () => onCloseRef.current();
    d.addEventListener("close", handle);
    return () => d.removeEventListener("close", handle);
  }, []);

  return (
    <dialog
      ref={ref}
      className={`modal${className ? ` ${className}` : ""}`}
      aria-labelledby={titleId}
      onClick={(e) => {
        // a click on the backdrop (outside the sheet) closes the dialog
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="modal__sheet panel panel--paper">
        <span className="panel__corner panel__corner--tl" aria-hidden />
        <span className="panel__corner panel__corner--tr" aria-hidden />
        <span className="panel__corner panel__corner--bl" aria-hidden />
        <span className="panel__corner panel__corner--br" aria-hidden />
        <header className="modal__head">
          <PixelSprite map={SEAL} scale={3} className="modal__seal" />
          <div>
            <h2 className="modal__title" id={titleId}>{title}</h2>
            {subtitle ? <p className="modal__subtitle">{subtitle}</p> : null}
          </div>
          <button type="button" className="btn btn--icon" onClick={onClose} aria-label="Close (Esc)">
            <span aria-hidden>×</span>
          </button>
        </header>
        <div className="modal__body">{open ? children : null}</div>
      </div>
    </dialog>
  );
}

import React, { useEffect, useId, useRef } from 'react';
import { X } from 'lucide-react';
export function IconButton({ label, children, ...props }) {
  return (
    <button
      type="button"
      className="icon-btn"
      aria-label={label}
      title={label}
      {...props}
    >
      {children}
    </button>
  );
}
export function Toggle({ value, onChange, label }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={value}
      aria-label={label}
      className={'toggle ' + (value ? 'on' : '')}
      onClick={() => onChange(!value)}
    >
      <span />
    </button>
  );
}
export function Modal({ title, kicker, children, close, wide = false }) {
  const r = useRef(null),
    cb = useRef(close),
    id = useId();
  cb.current = close;
  useEffect(() => {
    const prev = document.activeElement,
      before = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    const focusables = () =>
      Array.from(
        r.current.querySelectorAll(
          'button:not([disabled]),input:not([disabled]),a[href],select,[tabindex="0"]',
        ),
      );
    focusables()[0]?.focus();
    const key = (e) => {
      if (e.key === 'Escape') {
        e.stopPropagation();
        cb.current();
      }
      if (e.key === 'Tab') {
        const list = focusables(),
          first = list[0],
          last = list.at(-1);
        if (e.shiftKey && document.activeElement === first) {
          e.preventDefault();
          last?.focus();
        } else if (!e.shiftKey && document.activeElement === last) {
          e.preventDefault();
          first?.focus();
        }
      }
    };
    document.addEventListener('keydown', key);
    return () => {
      document.body.style.overflow = before;
      document.removeEventListener('keydown', key);
      prev?.focus?.();
    };
  }, []);
  return (
    <div
      className="modal-backdrop"
      onMouseDown={(e) => {
        if (e.currentTarget === e.target) close();
      }}
    >
      <section
        ref={r}
        role="dialog"
        aria-modal="true"
        aria-labelledby={id}
        className={'modal ' + (wide ? 'wide' : '')}
      >
        <div className="modal-head">
          <div>
            <div className="eyebrow">{kicker || 'PETDESK STUDIO'}</div>
            <h2 id={id}>{title}</h2>
          </div>
          <IconButton label="关闭" onClick={close}>
            <X />
          </IconButton>
        </div>
        {children}
      </section>
    </div>
  );
}
export function SnackArt() {
  return (
    <svg
      className="snack-art"
      width="68"
      height="68"
      viewBox="0 0 72 72"
      aria-hidden="true"
    >
      <ellipse cx="36" cy="60" rx="24" ry="4" fill="#dcd5ed" />
      <g transform="rotate(-12 35 36)">
        <path
          d="M17 14q0-3 4-3h19l10 11v30q0 4-4 4H21q-4 0-4-4Z"
          fill="#dfd7f8"
          stroke="#aa99d7"
          strokeWidth="1.5"
        />
        <path
          d="M39 12v10h10M23 28h16m-16 6h12"
          stroke="#a697d1"
          fill="none"
          strokeWidth="2"
          strokeLinecap="round"
        />
      </g>
      <path
        d="M51 26q-1 7 6 9-3 6 3 9-2 15-16 16-16 1-20-13-4-15 11-21 9-4 16 0Z"
        fill="#e9b479"
        stroke="#bc895b"
        strokeWidth="1.5"
      />
      <path
        d="M48 25q-1 7 6 9-3 6 3 9-2 13-15 14-15 0-18-12-2-14 11-19 6-3 13-1Z"
        fill="#ffdc9e"
      />
      <g fill="#b78166">
        <circle cx="36" cy="33" r="2.2" />
        <circle cx="31" cy="45" r="2" />
        <circle cx="45" cy="44" r="2.7" />
        <circle cx="39" cy="51" r="1.5" />
        <circle cx="48" cy="36" r="1.4" />
      </g>
      <path
        d="m58 13 1.3 4.2L64 19l-4.7 1.6L58 25l-1.4-4.4L52 19l4.6-1.8Z"
        fill="#aa93d9"
      />
      <circle cx="11" cy="43" r="2" fill="#bca9e1" />
    </svg>
  );
}

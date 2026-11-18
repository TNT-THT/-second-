import React, { useEffect, useRef, useState } from 'react';
import clips from '../core/qwen-atlas.json';
// Only a handful of decoded textures, not one DOM image per frame. Share atlas loads in a window.
const cache = new Map();
function sheet(key) {
  if (cache.has(key)) {
    const p = cache.get(key);
    cache.delete(key);
    cache.set(key, p);
    return p;
  }
  const im = new Image();
  im.src = `./art/motion/${key}.webp`;
  const p = im
    .decode()
    .then(() => im)
    .catch((e) => {
      cache.delete(key);
      throw e;
    });
  cache.set(key, p);
  if (cache.size > 3) cache.delete(cache.keys().next().value);
  return p;
}
export function QwenActor({
  kind = 'idle',
  face = 1,
  trackRef,
  demo = false,
  token = 0,
}) {
  const el = useRef(null),
    paint = useRef(null),
    control = useRef(null);
  const [failed, setFailed] = useState(false);
  control.current = { face, trackRef, demo };
  useEffect(() => {
    let valid = true;
    const key = Object.hasOwn(clips, kind) ? kind : 'idle';
    sheet(key)
      .then((im) => {
        if (valid) {
          paint.current = {
            key,
            im,
            clip: clips[key],
            start: performance.now(),
            last: -1,
          };
          setFailed(false);
          // Warm the likely next sheet while this pose is already on screen, especially
          // the short landing beat which should not be eaten up by a first-time decode.
          const next = {
            idle: 'walk',
            walk: 'climb',
            climb: 'glide',
            held: 'land',
          }[key];
          if (!control.current.demo && next) void sheet(next).catch(() => {});
        }
      })
      .catch(() => {
        if (valid) setFailed(true);
      });
    return () => {
      valid = false;
    };
  }, [kind, token]);
  useEffect(() => {
    let raf,
      prev = -1000;
    const mq = matchMedia('(prefers-reduced-motion: reduce)');
    function draw(t) {
      raf = requestAnimationFrame(draw);
      const a = paint.current,
        canvas = el.current;
      if (!canvas || !a || document.hidden || t - prev < 1000 / 30) return;
      prev = t;
      const { clip, im, key } = a,
        { trackRef, face, demo } = control.current;
      let phase = (t - a.start) / 1000 / clip.seconds;
      const motion = trackRef?.current;
      if (
        !demo &&
        motion?.kind === key &&
        ['walk', 'climb', 'glide'].includes(key)
      )
        phase = motion.phase;
      if (key === 'land' && !demo) phase = Math.min(0.999, Math.max(0, phase));
      const n =
          mq.matches && !demo && ['idle', 'sleep', 'notice'].includes(key)
            ? 0
            : Math.floor((((phase % 1) + 1) % 1) * clip.frames),
        flip = face < 0;
      if (a.last === n && a.flip === flip) return;
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, clip.width, clip.height);
      if (flip) ctx.setTransform(-1, 0, 0, 1, clip.width, 0);
      ctx.drawImage(
        im,
        (n % clip.cols) * clip.width,
        Math.floor(n / clip.cols) * clip.height,
        clip.width,
        clip.height,
        0,
        0,
        clip.width,
        clip.height,
      );
      canvas.dataset.clip = key;
      canvas.dataset.frame = String(n);
      canvas.dataset.face = flip ? 'left' : 'right';
      a.last = n;
      a.flip = flip;
    }
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, []);
  return failed ? (
    <img
      className="avatar"
      src="./art/qwen.webp"
      alt="Qwen 娘，动作资源未能载入"
    />
  ) : (
    <canvas
      ref={el}
      width="352"
      height="480"
      className="avatar qwen-actor"
      role="img"
      aria-label="Qwen 娘动作立绘"
    />
  );
}

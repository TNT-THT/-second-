import React, { useId, useMemo, useRef, useLayoutEffect } from 'react';
import { drawChibi, poseAt } from '../art-src/chibi/puppet.mjs';
import { clips } from '../core/repertoire.mjs';
// Install trusted, authored paths once per costume, then update only joint attributes. No
// remote references, atlas gutters, image decodes or asynchronous costume races.
export function QwenActor({
  skin = 'qwen',
  kind = 'idle',
  face = 1,
  trackRef,
  demo = false,
  token = 0,
}) {
  const id = useId(),
    host = useRef(null),
    ctl = useRef(null);
  ctl.current = { kind, face, trackRef, demo, token };
  const markup = useMemo(
    () => ({ __html: drawChibi({ skin, uid: id }) }),
    [skin, id],
  );
  useLayoutEffect(() => {
    const el = host.current,
      parts = Array.from(el.querySelectorAll('[data-j]')).map((n) => [
        n.dataset.j,
        n,
      ]),
      cache = {},
      mq = matchMedia('(prefers-reduced-motion: reduce)');
    let start = 0,
      lastKey = '';
    function draw(t) {
      if (document.hidden) return;
      const cfg = ctl.current,
        k = Object.hasOwn(clips, cfg.kind) ? cfg.kind : 'idle',
        key = k + '/' + cfg.token;
      if (key !== lastKey) {
        lastKey = key;
        start = t;
      }
      let p = ((t - start) / 1000 / clips[k].seconds) % 1;
      const m = cfg.trackRef?.current;
      if (
        !cfg.demo &&
        m?.kind === k &&
        ['walk', 'climb', 'hang', 'land'].includes(k)
      )
        p = m.phase;
      if (!cfg.demo && mq.matches && ['idle', 'notice', 'sleep'].includes(k))
        p = 0.18;
      const state = poseAt(k, p, cfg.face);
      for (const [name, node] of parts) {
        const value = state[name];
        if (cache[name] === value) continue;
        if (value === '!') node.setAttribute('display', 'none');
        else {
          if (cache[name] == null || cache[name] === '!')
            node.removeAttribute('display');
          if (value) node.setAttribute('transform', value);
          else node.removeAttribute('transform');
        }
        cache[name] = value;
      }
      el.dataset.clip = k;
      el.dataset.face = cfg.face < 0 ? 'left' : 'right';
      el.dataset.frame = String(Math.floor(p * 48));
    }
    draw(performance.now());
    const timer = setInterval(() => draw(performance.now()), 34);
    return () => clearInterval(timer);
  }, [markup]);
  return (
    <span
      ref={host}
      className="avatar qwen-actor"
      role="img"
      aria-label="Q版 Qwen 娘动作立绘"
      data-skin={skin}
      dangerouslySetInnerHTML={markup}
    />
  );
}

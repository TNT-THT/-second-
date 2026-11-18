import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { Box, TriangleAlert, Heart, Cookie } from 'lucide-react';
import { getModel } from './models.js';
import { act, api, chirp, useStudio } from './store.js';
import { advance, confine, destination } from '../core/motion.mjs';
import { watchAlpha } from './hit-mask.js';
export function Avatar({ model, className = '', flat = false }) {
  const [asset, setAsset] = useState(null),
    [error, setError] = useState('');
  const canvas = useRef(null);
  useEffect(() => {
    setAsset(null);
    setError('');
    if (model.id === 'qwen' || (flat && model.kind === 'glb')) return;
    let live = true,
      url;
    getModel(model.id)
      .then((r) => {
        if (!live) return;
        if (!r) throw Error('模型副本缺失');
        if (r.kind === 'image') {
          url = URL.createObjectURL(r.blob);
          setAsset({ id: r.id, url });
        } else setAsset({ id: r.id, blob: r.blob });
      })
      .catch((e) => live && setError(e.message));
    return () => {
      live = false;
      if (url) URL.revokeObjectURL(url);
    };
  }, [model.id, model.kind, flat]);
  useEffect(() => {
    if (!asset?.blob || asset.id !== model.id || !canvas.current) return;
    const el = canvas.current,
      c = new AbortController();
    Promise.all([import('./three-model.js'), asset.blob.arrayBuffer()])
      .then(([m, b]) => {
        if (!c.signal.aborted) return m.mountModel(el, b, c.signal);
      })
      .catch(() => {
        if (!c.signal.aborted) setError('3D 渲染不可用');
      });
    return () => c.abort();
  }, [asset, model.id]);
  if (error)
    return (
      <span
        role="img"
        aria-label={error}
        className={'model-error ' + className}
      >
        <TriangleAlert size={20} />
        <small>{error}</small>
      </span>
    );
  if (model.id === 'qwen')
    return (
      <img
        draggable="false"
        className={'avatar ' + className}
        src={flat ? './art/qwen-face.webp' : './art/qwen.webp'}
        alt="蓝发执扇的 Qwen 娘"
      />
    );
  if (model.kind === 'glb')
    return flat ? (
      <Box className={'avatar box-avatar ' + className} />
    ) : (
      <canvas
        className={'avatar ' + className}
        ref={canvas}
        aria-label={model.name + ' 3D 模型'}
      />
    );
  return asset?.id === model.id ? (
    <img
      draggable="false"
      className={'avatar ' + className}
      src={asset.url}
      alt={model.name}
    />
  ) : (
    <span />
  );
}
export function Companion({ mode = 'stage', onFeed, notify, paused = false }) {
  const { state } = useStudio();
  const plane = useRef(null),
    node = useRef(null),
    pos = useRef({ x: 0, y: 0 }),
    bounds = useRef(null),
    target = useRef(null),
    drag = useRef(null),
    hover = useRef(false),
    dwell = useRef(0),
    config = useRef();
  const [walking, walk] = useState(false),
    [held, setHeld] = useState(false),
    [jump, setJump] = useState(false),
    [drop, setDrop] = useState(false),
    [tick, force] = useState(0);
  const { prefs, reaction, sleeping, model } = state;
  config.current = { prefs, sleeping, paused, until: reaction?.until || 0 };
  const native = mode === 'native';
  function place(p) {
    pos.current = p;
    const n = node.current,
      b = bounds.current;
    if (n) {
      n.style.left = p.x + 'px';
      n.style.top = p.y + 'px';
      n.dataset.flip = p.y < 37 ? 'true' : 'false';
      if (b) {
        const cx = p.x + b.w / 2;
        n.style.setProperty(
          '--bubble-shift',
          Math.max(0, 85 - cx) - Math.max(0, cx - b.width + 85) + 'px',
        );
      }
    }
  }
  useEffect(() => {
    if (!reaction) return;
    const timer = setTimeout(
      () => force((t) => t + 1),
      Math.max(0, reaction.until - Date.now() + 20),
    );
    return () => clearTimeout(timer);
  }, [reaction]);
  useLayoutEffect(() => {
    if (native) return;
    let raf,
      last = 0,
      was = false,
      initialized = false,
      jumpTimer;
    function measure() {
      if (!node.current || !plane.current) return;
      const p = plane.current,
        n = node.current;
      const old = bounds.current;
      const b = {
        x: 0,
        y: 0,
        width: p.clientWidth,
        height: p.clientHeight,
        w: n.offsetWidth,
        h: n.offsetHeight,
      };
      const fx =
          initialized && old?.width > old.w
            ? pos.current.x / (old.width - old.w)
            : mode === 'float'
              ? 0.78
              : 0.495,
        fy =
          initialized && old?.height > old.h
            ? pos.current.y / (old.height - old.h)
            : mode === 'float'
              ? 0.77
              : 0.85;
      bounds.current = b;
      place(confine(fx * (b.width - b.w), fy * (b.height - b.h), b, b.w, b.h));
      target.current = null;
      dwell.current = performance.now() + 7000;
      initialized = true;
    }
    measure();
    const ro = new ResizeObserver(measure);
    ro.observe(plane.current);
    ro.observe(node.current);
    function run(t) {
      raf = requestAnimationFrame(run);
      if (t - last < 35) return;
      const dt = (t - last) / 1000;
      last = t;
      const c = config.current,
        b = bounds.current;
      let moving = false;
      if (
        !document.hidden &&
        b?.width &&
        c.prefs.roam &&
        !c.sleeping &&
        !c.paused &&
        !drag.current &&
        !hover.current &&
        t > dwell.current &&
        Date.now() > c.until
      ) {
        if (!target.current) {
          // Keep the habitat's feet on the terrace, not in the painted sky.
          const ground =
            mode === 'stage'
              ? {
                  ...b,
                  y: (b.height - b.h) * 0.72,
                  height: b.height - (b.height - b.h) * 0.72,
                }
              : b;
          target.current = destination(
            pos.current,
            ground,
            b.w,
            b.h,
            c.prefs.edges,
          );
        }
        const next = advance(
          pos.current,
          target.current,
          mode === 'stage' ? 22 : 52,
          dt,
        );
        place(next);
        moving = !next.arrived;
        if (next.arrived) {
          target.current = null;
          dwell.current = t + 2600 + Math.random() * 3900;
          if (c.prefs.edges && Math.random() < 0.5) {
            setJump(true);
            jumpTimer = setTimeout(() => setJump(false), 900);
          }
        }
      } else if (
        target.current &&
        (c.sleeping || !c.prefs.roam || drag.current)
      )
        target.current = null;
      if (moving !== was) {
        walk(moving);
        was = moving;
      }
    }
    raf = requestAnimationFrame(run);
    return () => {
      cancelAnimationFrame(raf);
      ro.disconnect();
      clearTimeout(jumpTimer);
    };
  }, [mode, native]);
  useEffect(() => {
    if (!native) return;
    let tm;
    const off = api.onMotion((m) => {
      walk(!!m.walking);
      if (m.play) {
        setJump(true);
        clearTimeout(tm);
        tm = setTimeout(() => setJump(false), 900);
      }
    });
    return () => {
      off();
      clearTimeout(tm);
    };
  }, [native]);
  useEffect(() => {
    if (native && !prefs.passThrough)
      return watchAlpha(plane.current, api.mask);
  }, [native, prefs.passThrough]);
  function pat() {
    chirp();
    void act({ type: 'pat' });
  }
  function down(e) {
    if (e.button !== 0 || drag.current) return;
    e.preventDefault();
    e.currentTarget.setPointerCapture(e.pointerId);
    drag.current = {
      sx: e.screenX,
      sy: e.screenY,
      x: pos.current.x,
      y: pos.current.y,
      moved: false,
    };
    setHeld(true);
    target.current = null;
    if (native) void api.drag(true);
  }
  function move(e) {
    const d = drag.current;
    if (!d) return;
    const dx = e.screenX - d.sx,
      dy = e.screenY - d.sy;
    if (Math.hypot(dx, dy) > 4) d.moved = true;
    if (!native && d.moved && bounds.current) {
      const b = bounds.current;
      place(confine(d.x + dx, d.y + dy, b, b.w, b.h));
    }
  }
  function up(e, cancel = false) {
    const d = drag.current;
    if (!d) return;
    drag.current = null;
    setHeld(false);
    if (native) void api.drag(false);
    if (e.currentTarget.hasPointerCapture(e.pointerId))
      e.currentTarget.releasePointerCapture(e.pointerId);
    dwell.current = performance.now() + 3000;
    if (!cancel && !d.moved) pat();
  }
  const live = reaction && reaction.until > Date.now() ? reaction : null;
  const kind = held
    ? 'held'
    : sleeping
      ? 'sleep'
      : live?.kind || (jump ? 'play' : walking ? 'walk' : 'idle');
  const speech = drop
    ? '啊，是我的点心吗？'
    : held
      ? '要去哪里呀？'
      : live?.text ||
        (sleeping
          ? '呼… z Z'
          : walking
            ? '去那边逛逛～'
            : '今天也请多多关照。');
  return (
    <div
      ref={plane}
      className={`pet-plane ${mode} ${drop ? 'drag-over' : ''}`}
      onDragOver={(e) => {
        if (e.dataTransfer.types.includes('Files')) {
          e.preventDefault();
          e.dataTransfer.dropEffect = 'copy';
          setDrop(true);
        }
      }}
      onDragLeave={(e) => {
        if (!e.currentTarget.contains(e.relatedTarget)) setDrop(false);
      }}
      onDrop={(e) => {
        e.preventDefault();
        e.stopPropagation();
        setDrop(false);
        onFeed?.(e.dataTransfer.files, e.dataTransfer.items);
      }}
      onContextMenu={(e) => {
        if (native) {
          e.preventDefault();
          void api.menu();
        }
      }}
    >
      <div
        ref={node}
        role="button"
        tabIndex="0"
        aria-label={`${model.name}，点击摸头，拖动移动`}
        className={'pet-node ' + (held ? 'held' : '')}
        style={{ '--pet-scale': prefs.scale }}
        onPointerDown={down}
        onPointerMove={move}
        onPointerUp={up}
        onPointerCancel={(e) => up(e, true)}
        onLostPointerCapture={(e) => up(e, true)}
        onPointerEnter={() => {
          hover.current = true;
        }}
        onPointerLeave={() => {
          hover.current = false;
        }}
        onKeyDown={(e) => {
          if (['Enter', ' '].includes(e.key)) {
            e.preventDefault();
            pat();
          }
        }}
        onContextMenu={(e) => {
          if (mode === 'float') {
            e.preventDefault();
            notify?.('点击左下角「收回」即可返回工作台。');
          }
        }}
      >
        {(!native || ((live || held || drop) && !prefs.passThrough)) && (
          <div className="pet-speech">
            {speech}
            {!live && !sleeping && !walking && !held && !drop && (
              <span className="speech-heart">♥</span>
            )}
          </div>
        )}
        <span className="pet-shadow" />
        <div className={'pet-art ani-' + kind}>
          <Avatar model={model} />
        </div>
        {(kind === 'pat' || kind === 'eat') && (
          <span className="pet-emote" key={state.reaction?.id}>
            {kind === 'eat' ? (
              <Cookie size={22} />
            ) : (
              <Heart size={22} fill="#e997b1" />
            )}
          </span>
        )}
      </div>
    </div>
  );
}

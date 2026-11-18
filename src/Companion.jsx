import { builtin, motionOptions } from '../core/repertoire.mjs';
import { say } from '../core/dialogue.mjs';
import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { Box, TriangleAlert, Heart } from 'lucide-react';
import { getModel } from './models.js';
import { act, api, chirp, useStudio } from './store.js';
import {
  actorSize,
  confine,
  resizeAnchored,
  STILL,
  Wander,
} from '../core/motion.mjs';
import { performanceFor } from '../core/performance.mjs';
import { QwenActor } from './QwenActor.jsx';
export function Avatar({
  model,
  className = '',
  flat = false,
  animated = false,
  kind = 'idle',
  face = 1,
  trackRef,
  token = 0,
  demo = false,
  disabled = [],
}) {
  const actionRef = useRef({ kind, face });
  actionRef.current = { kind, face, disabled };
  const [asset, setAsset] = useState(null),
    [error, setError] = useState('');
  const canvas = useRef(null);
  useEffect(() => {
    setAsset(null);
    setError('');
    if (builtin(model.id)) return;
    let live = true,
      url;
    getModel(model.id)
      .then((r) => {
        if (!live) return;
        if (!r) throw Error('模型副本缺失');
        if (r.kind === 'glb' && model.flyClip !== r.flyClip)
          void act({ type: 'model-caps', id: r.id, flyClip: r.flyClip });
        if (r.kind === 'image' || (flat && r.thumb)) {
          url = URL.createObjectURL(flat && r.thumb ? r.thumb : r.blob);
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
        if (!c.signal.aborted)
          return m.mountModel(el, b, c.signal, () => actionRef.current);
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
  if (builtin(model.id) && animated)
    return (
      <QwenActor
        skin={model.id}
        demo={demo}
        kind={kind}
        face={face}
        trackRef={trackRef}
        token={token}
      />
    );
  if (builtin(model.id))
    return (
      <img
        draggable="false"
        className={'avatar ' + className}
        src={`./art/${model.id}${flat ? '-face' : ''}.svg`}
        alt="蓝发执扇的 Qwen 娘"
      />
    );
  if (model.kind === 'glb' && !(flat && asset?.url && asset.id === model.id))
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
export function Companion({
  mode = 'stage',
  onFeed,
  notify,
  paused = false,
  practice = false,
}) {
  const { state } = useStudio(),
    { prefs, reaction, sleeping, model } = state,
    native = mode === 'native';
  const plane = useRef(null),
    node = useRef(null),
    rig = useRef(null),
    measure = useRef(null),
    drag = useRef(null),
    trackRef = useRef({ ...STILL });
  const config = useRef(null),
    pos = useRef({ x: 0, y: 0 }),
    timers = useRef({}),
    hoverRef = useRef(false),
    dropRef = useRef(false);
  const [track, setTrack] = useState(STILL),
    [held, setHeld] = useState(false),
    [hover, setHover] = useState(false),
    [drop, setDrop] = useState(false),
    [, refresh] = useState(0);
  config.current = { state, prefs, sleeping, reaction, paused, practice };
  const bar = useRef(null);
  function platform(a, size) {
    if (!config.current.practice || !config.current.prefs.ledges) return null;
    return {
      id: 'practice',
      x: a.width * 0.13,
      width: a.width * 0.51,
      y: Math.min(a.height - 5, Math.max(size.height + 3, a.height * 0.72)),
    };
  }
  function sync(t) {
    if (!t) return;
    const last = trackRef.current;
    trackRef.current = t;
    if (
      last.kind !== t.kind ||
      last.face !== t.face ||
      last.surface !== t.surface ||
      last.moving !== t.moving
    )
      setTrack(t);
  }
  function place(p) {
    pos.current = { ...p };
    const el = node.current,
      b = rig.current;
    if (!el || !b) return;
    el.style.left = Math.round(p.x) + 'px';
    el.style.top = Math.round(p.y) + 'px';
    el.dataset.flip = p.y < 35 ? 'true' : 'false';
    const cx = p.x + b.size.width / 2;
    el.style.setProperty(
      '--bubble-shift',
      Math.max(0, 82 - cx) - Math.max(0, cx - b.area.width + 82) + 'px',
    );
  }
  useEffect(() => {
    if (!reaction) return;
    const tm = setTimeout(
      () => refresh((v) => v + 1),
      Math.max(0, reaction.until - Date.now() + 12),
    );
    return () => clearTimeout(tm);
  }, [reaction]);
  useEffect(
    () => () => {
      for (const t of Object.values(timers.current)) clearTimeout(t);
    },
    [],
  );
  useLayoutEffect(() => {
    if (native) return;
    const el = node.current,
      p = plane.current;
    let timer,
      last = performance.now();
    function fit() {
      if (!p.clientWidth || !p.clientHeight) return;
      const a = { x: 0, y: 0, width: p.clientWidth, height: p.clientHeight },
        base = p.closest('.expanded')
          ? 258
          : window.innerWidth < 760
            ? Math.min(184, Math.max(145, window.innerWidth * 0.32))
            : 225;
      const size = actorSize(
        config.current.prefs.scale,
        a,
        base,
        Math.min(39, a.height * 0.13),
      );
      el.style.height = size.height + 'px';
      el.style.width = size.width + 'px';
      const shelf = platform(a, size);
      if (bar.current && shelf) {
        Object.assign(bar.current.style, {
          left: shelf.x + 'px',
          top: shelf.y + 'px',
          width: shelf.width + 'px',
        });
      }
      const old = rig.current;
      if (
        old &&
        old.area.width === a.width &&
        old.area.height === a.height &&
        old.size.width === size.width &&
        old.size.height === size.height
      )
        return;
      if (!old) {
        const pos = {
          x: (a.width - size.width) * 0.495,
          y: a.height - size.height,
        };
        rig.current = new Wander(pos, a, size, { surface: 'floor', wait: 6 });
      } else {
        const oldbox = { ...old.pos, ...old.size };
        let n = resizeAnchored(oldbox, size, a, old.surface);
        if (old.area.width !== a.width || old.area.height !== a.height) {
          n = confine(
            ((a.width - size.width) * old.pos.x) /
              Math.max(1, old.area.width - old.size.width),
            ((a.height - size.height) * old.pos.y) /
              Math.max(1, old.area.height - old.size.height),
            a,
            size.width,
            size.height,
          );
        }
        old.place(n, a, size, { surface: old.surface, wait: 1, ledge: shelf });
      }
      place(rig.current.pos);
      sync(rig.current.track());
    }
    fit();
    measure.current = fit;
    const ro = new ResizeObserver(fit);
    ro.observe(p);
    function run() {
      // Transparent/occluded compositors may withhold frame callbacks. Keep physics on
      // a monotonic clock like the native main process; never catch up a hidden tab.
      const t = performance.now();
      const dt = (t - last) / 1000;
      last = t;
      const r = rig.current,
        c = config.current;
      if (!r || document.hidden || drag.current) return;
      const pose = r.step(dt, {
        ...motionOptions(c.state),
        ledge: platform(r.area, r.size),
        blocked:
          c.sleeping ||
          hoverRef.current ||
          dropRef.current ||
          c.paused ||
          c.reaction?.until > Date.now(),
      });
      if (r.pos.x !== pos.current.x || r.pos.y !== pos.current.y) place(r.pos);
      sync(pose);
    }
    timer = setInterval(run, 33);
    return () => {
      clearInterval(timer);
      ro.disconnect();
      measure.current = null;
    };
  }, [native]);
  useLayoutEffect(() => {
    measure.current?.();
  }, [prefs.scale, practice, prefs.ledges]);
  useEffect(() => {
    if (!native) return;
    let alive = true,
      seen = false;
    const off = api.onMotion((t) => {
      seen = true;
      sync(t);
    });
    api
      .snapshot()
      .then((v) => {
        if (alive && !seen) sync(v.motion);
      })
      .catch(() => {});
    return () => {
      alive = false;
      off();
    };
  }, [native]);
  const pat = () => {
    chirp('pat');
    void act({ type: 'pat' });
  };
  function down(e) {
    if (e.button !== 0 || drag.current) return;
    e.preventDefault();
    e.currentTarget.setPointerCapture(e.pointerId);
    const d = {
      id: e.pointerId,
      sx: e.screenX,
      sy: e.screenY,
      x: pos.current.x,
      y: pos.current.y,
      moved: false,
      long: false,
      lastX: e.screenX,
      lastAt: performance.now(),
    };
    drag.current = d;

    rig.current?.stop();
    timers.current.hold = setTimeout(() => {
      if (drag.current === d) {
        d.long = true;
        setHeld(true);
      }
    }, 180);
    if (native) void api.drag(true);
  }
  function movePointer(e) {
    const d = drag.current;
    if (!d || e.pointerId !== d.id) return;
    const dx = e.screenX - d.sx,
      dy = e.screenY - d.sy;
    if (Math.hypot(dx, dy) >= 4) {
      d.moved = true;
      setHeld(true);
    }
    if (!d.moved) return;
    const t = performance.now(),
      vx = (e.screenX - d.lastX) / Math.max(12, t - d.lastAt);
    d.lastX = e.screenX;
    d.lastAt = t;
    node.current.style.setProperty(
      '--lift-tilt',
      Math.max(-7, Math.min(7, -vx * 5)) + 'deg',
    );
    clearTimeout(timers.current.tilt);
    timers.current.tilt = setTimeout(
      () => node.current?.style.setProperty('--lift-tilt', '0deg'),
      100,
    );
    if (!native && rig.current) {
      const r = rig.current;
      r.pos = confine(d.x + dx, d.y + dy, r.area, r.size.width, r.size.height);
      place(r.pos);
    }
  }
  function up(e, cancel = false) {
    const d = drag.current;
    if (!d || e.pointerId !== d.id) return;
    drag.current = null;
    clearTimeout(timers.current.hold);
    setHeld(false);
    if (native) void api.drag(false);
    if (e.currentTarget.hasPointerCapture(e.pointerId))
      e.currentTarget.releasePointerCapture(e.pointerId);
    const r = rig.current;
    if (r && (d.moved || d.long)) {
      r.place(r.pos, r.area, r.size, {
        ...motionOptions(state),
        wait: 1.1,
        snap: d.moved,
        release: true,
        ledge: platform(r.area, r.size),
      });
      place(r.pos);
      sync(r.track());
    }
    if (!d.moved && !d.long && !cancel) pat();
  }
  function overFiles(on) {
    dropRef.current = on;
    setDrop(on);
  }
  const live = reaction && reaction.until > Date.now() ? reaction : null;
  const kind = performanceFor({
    track,
    held,
    prefs,
    offer: drop,
    hover,
    sleeping,
    reaction: live,
  });
  const face = ['walk', 'climb', 'hang', 'fly'].includes(kind) ? track.face : 1;
  const cue = drop
    ? 'offer'
    : held
      ? 'held'
      : track.surface === 'window' && kind === 'idle'
        ? 'window'
        : kind;
  const counts = useRef({}),
    [utter, setUtter] = useState({ text: say('idle'), brief: false });
  useEffect(() => {
    const n = counts.current[cue] || 0;
    counts.current[cue] = n + 1;
    const brief = [
      'land',
      'held',
      'offer',
      'notice',
      'read',
      'tea',
      'play',
      'window',
      'fly',
    ].includes(cue);
    const text = say(cue, n);
    setUtter({ text, brief });
    if (brief) {
      const timer = setTimeout(() => setUtter({ text, brief: false }), 2400);
      return () => clearTimeout(timer);
    }
  }, [cue]);
  const speech = (!drop && !held && live?.text) || utter.text;
  return (
    <div
      ref={plane}
      className={`pet-plane ${mode} ${drop ? 'drag-over' : ''}`}
      onDragOver={(e) => {
        if (e.dataTransfer.types.includes('Files')) {
          e.preventDefault();
          e.dataTransfer.dropEffect = 'copy';
          overFiles(true);
        }
      }}
      onDragLeave={(e) => {
        if (!e.currentTarget.contains(e.relatedTarget)) overFiles(false);
      }}
      onDrop={(e) => {
        e.preventDefault();
        e.stopPropagation();
        overFiles(false);
        onFeed?.(e.dataTransfer.files, e.dataTransfer.items);
      }}
      onContextMenu={(e) => {
        e.preventDefault();
        if (native) void api.menu();
        else notify?.('预览中可以点按或拖动角色；桌面版右键可打开伙伴菜单。');
      }}
    >
      {!native && practice && prefs.ledges && (
        <div className="practice-ledge" ref={bar}>
          <span>练习窗沿</span>
        </div>
      )}
      <div
        ref={node}
        role="button"
        tabIndex="0"
        aria-label={`${model.name}，点击摸头，拖动移动`}
        data-pose={kind}
        data-surface={track.surface}
        data-facing={face < 0 ? 'left' : 'right'}
        className={'pet-node ' + (held ? 'held' : '')}
        onPointerDown={down}
        onPointerMove={movePointer}
        onPointerUp={up}
        onPointerCancel={(e) => up(e, true)}
        onLostPointerCapture={(e) => up(e, true)}
        onPointerEnter={() => {
          hoverRef.current = true;
          setHover(true);
        }}
        onPointerLeave={() => {
          hoverRef.current = false;
          setHover(false);
        }}
        onKeyDown={(e) => {
          if (['Enter', ' '].includes(e.key)) {
            e.preventDefault();
            if (!e.repeat) pat();
          }
        }}
      >
        {(!native || live || drop || held || utter.brief) && (
          <div className="pet-speech">
            {speech}
            {!live && kind === 'idle' && (
              <span className="speech-heart">♥</span>
            )}
          </div>
        )}
        {['floor', 'window'].includes(track.surface) && kind !== 'held' && (
          <span className="pet-shadow" />
        )}
        <div
          className={
            'pet-art ' +
            (builtin(model.id)
              ? 'authored'
              : model.kind === 'image'
                ? 'image-adapter'
                : 'model-adapter')
          }
          style={{ '--facing': face }}
        >
          <Avatar
            model={model}
            key={model.id}
            disabled={prefs.disabledClips}
            animated
            kind={kind}
            face={face}
            trackRef={trackRef}
            token={live?.id || 0}
          />
        </div>
        {kind === 'pat' && (
          <span className="pet-emote" key={live?.id}>
            <Heart size={17} fill="#e7a1bc" />
          </span>
        )}
        {kind === 'eat' && (
          <span className="biscuit-crumbs" aria-hidden="true">
            <i />
            <i />
            <i />
          </span>
        )}
        {kind === 'sleep' && (
          <span className="sleep-letters" aria-hidden="true">
            z Z
          </span>
        )}
      </div>
    </div>
  );
}

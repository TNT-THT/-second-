import React, { useState } from 'react';
import {
  ArrowLeft,
  ArrowRight,
  BookOpen,
  Feather,
  Film,
  Footprints,
  LockKeyhole,
  SlidersHorizontal,
  Sparkles,
} from 'lucide-react';
import { Avatar } from './Companion.jsx';
import { act, pref } from './store.js';
import { Toggle } from './ui.jsx';
import { allowed, builtin, canFly, clips } from '../core/repertoire.mjs';
export function RoutePicker({ state }) {
  const p = state.prefs,
    cap = canFly(state.model),
    permit = allowed(p, 'fly'),
    modes = [
      [false, '步行巡游', Footprints],
      [true, '自由飞行', Feather],
    ];
  return (
    <div className="route-picker">
      <div className="route-tabs" role="group" aria-label="移动方式">
        {modes.map(([v, n, I]) => (
          <button
            key={n}
            disabled={v && (!cap || !permit)}
            aria-pressed={p.flight === v}
            onClick={() => pref('flight', v)}
          >
            <I size={15} />
            {n}
          </button>
        ))}
      </div>
      <small>
        {!cap
          ? '这个模型没有飞行片段，只能步行。'
          : !permit
            ? '飞行已禁用；在动作手册内允许后才可起飞。'
            : p.flight
              ? '在空中悬停；开启漫游后可向任意方向飞行。'
              : '松手后落到地面或窗沿；贴住边框时可攀行。'}
      </small>
    </div>
  );
}
const groups = [
  ['移动', ['walk', 'climb', 'hang', 'fly', 'held', 'fall', 'land']],
  ['待机', ['idle', 'play', 'read', 'tea', 'sleep']],
  ['互动', ['notice', 'pat', 'eat']],
];
export function MotionRoom({ state, compact = false }) {
  const [key, select] = useState('walk'),
    [face, turn] = useState(1);
  const clip = clips[key],
    p = state.prefs,
    m = state.model,
    cap = key !== 'fly' || canFly(m),
    enabled = allowed(p, key),
    show = cap && enabled ? key : 'idle',
    mirror = ['walk', 'climb', 'hang', 'fly'].includes(key),
    skin = builtin(m.id);
  return (
    <div
      className={
        'motion-room handbook ' + (compact ? 'compact-book' : 'full-book')
      }
    >
      <section className="book-player">
        <div className="rehearsal">
          <div className="reel-slate">
            <Film size={13} /> POCKET THEATRE
          </div>
          <div
            className="reel-figure audition-node"
            data-pose={show}
            style={{ '--facing': mirror ? face : 1 }}
          >
            <div
              className={
                'pet-art ' + (m.kind === 'image' ? 'image-adapter' : '')
              }
            >
              <Avatar
                key={m.id}
                model={m}
                animated
                kind={show}
                face={mirror ? face : 1}
                demo
                disabled={p.disabledClips}
              />
            </div>{' '}
          </div>
          <small>
            {skin?.code || 'CUSTOM'} &nbsp; / &nbsp;
            {String(Object.keys(clips).indexOf(key) + 1).padStart(2, '0')}
          </small>
        </div>
        <div className="book-copy">
          <div className="eyebrow">
            {skin ? `Q EDITION · ${skin.edition}` : m.name}
          </div>
          <div className="reel-head">
            <h3>{clip.label}</h3>
            {mirror && (
              <div className="dir-toggle">
                {[
                  [-1, ArrowLeft, '向左'],
                  [1, ArrowRight, '向右'],
                ].map(([v, I, n]) => (
                  <button
                    key={v}
                    aria-pressed={v === face}
                    onClick={() => turn(v)}
                  >
                    <I size={13} />
                    {n}
                  </button>
                ))}
              </div>
            )}
          </div>
          <p className="reel-copy">
            {!cap
              ? '当前模型没有飞行片段。'
              : !enabled
                ? '此动作已禁用，人物保持待机。'
                : clip.desc}
          </p>
          <div className="clip-permission">
            {clip.optional ? (
              <>
                <span>
                  <SlidersHorizontal size={15} />
                  允许此动作
                </span>
                <Toggle
                  label={'允许' + clip.label}
                  value={enabled}
                  onChange={(v) => act({ type: 'animation', key, enabled: v })}
                />
              </>
            ) : (
              <span>
                <LockKeyhole size={14} /> 基础动作 · 始终保留
              </span>
            )}
          </div>
          {!skin && (
            <small className="custom-note">
              外部 GLB 使用模型自带片段；静态图只有通用反馈。
            </small>
          )}
        </div>
      </section>
      <section className="book-index">
        <div className="book-caption">
          <BookOpen size={17} />
          <b>动作目录</b>
          <small>15 TAKES</small>
        </div>
        {groups.map(([title, keys]) => (
          <div className="take-group" key={title}>
            <strong>{title}</strong>
            <div>
              {keys.map((k) => (
                <button
                  key={k}
                  aria-pressed={k === key}
                  className={!allowed(p, k) ? 'off' : ''}
                  onClick={() => select(k)}
                >
                  {clips[k].label}
                  {!allowed(p, k) && <i aria-hidden="true">关</i>}
                </button>
              ))}
            </div>
          </div>
        ))}
        <p className="book-tip">
          选择动作试映；关闭「允许此动作」也会更新真实桌宠，不会只隐藏预览。
        </p>
      </section>
      {!compact && (
        <section className="book-schedule">
          <div className="book-caption">
            <Sparkles size={17} />
            <b>日常安排</b>
            <small>ROUTINE</small>
          </div>
          <div className="schedule-grid">
            <div>
              <h4>移动方式</h4>
              <RoutePicker state={state} />
            </div>
            <div>
              <h4>停下时做什么</h4>
              <LeisurePicker state={state} />
            </div>
          </div>
        </section>
      )}
    </div>
  );
}
export function LeisurePicker({ state }) {
  const p = state.prefs;
  return (
    <div className="leisure-picker">
      <select
        aria-label="待机活动"
        value={p.leisure}
        onChange={(e) => pref('leisure', e.target.value)}
      >
        {[
          ['mix', '随机消遣'],
          ['read', '翻翻绘本'],
          ['tea', '桂花茶歇'],
          ['play', '花间扇舞'],
          ['quiet', '安静待机'],
        ].map(([k, n]) => (
          <option key={k} value={k} disabled={!allowed(p, k)}>
            {n}
          </option>
        ))}
      </select>
      <small>站稳后才开始；关闭自由漫游也能消遣。</small>
    </div>
  );
}

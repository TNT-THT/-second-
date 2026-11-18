import test from 'node:test';
import assert from 'node:assert/strict';
import {
  initialState,
  reduceState,
  restoreState,
  foodHint,
} from '../core/state.mjs';
import { advance, confine, destination } from '../core/motion.mjs';
import { shouldIgnore, MASK_W, MASK_H } from '../core/hit-test.mjs';
test('actions change bounded stats without mutating prior state', () => {
  const old = initialState(1);
  let s = old;
  for (let i = 0; i < 125; i++) s = reduceState(s, { type: 'play' }, i);
  assert.deepEqual(old.stats, { fullness: 68, mood: 86, bond: 32 });
  assert.deepEqual(s.stats, { fullness: 0, mood: 100, bond: 100 });
  assert.equal(s.log.length, 100);
  assert.equal(s.reaction.kind, 'play');
});
test('nap, wake, successful meal, journal privacy', () => {
  let s = reduceState(initialState(), { type: 'nap' });
  assert.equal(s.sleeping, true);
  s = reduceState(s, { type: 'meal', path: '/secret/tax.pdf' });
  assert.equal(s.sleeping, false);
  assert.equal(s.meals, 1);
  assert.equal(s.stats.fullness, 84);
  assert.ok(!JSON.stringify(s.log).includes('tax.pdf'));
  assert.equal(reduceState(s, { type: 'clear-log' }).log.length, 0);
  assert.match(reduceState(s, { type: 'demo-meal' }).log[0].text, /模拟/);
});
test('unrecognized commands and invalid preferences have no effect', () => {
  const s = initialState();
  for (const a of [
    null,
    { type: 'spawn' },
    { type: 'pref', key: 'unknown', value: 1 },
    { type: 'pref', key: 'scale', value: 99 },
    { type: 'select', model: { id: '../../file', name: 'bad', kind: 'glb' } },
    { type: 'pref', key: 'passThrough', value: 'true' },
  ])
    assert.equal(reduceState(s, a), s);
});
test('valid saved choices survive restart; invalid data falls back', () => {
  const s = reduceState(initialState(3), {
    type: 'pref',
    key: 'passThrough',
    value: true,
  });
  assert.equal(restoreState(s).prefs.passThrough, true);
  assert.deepEqual(restoreState({}, 2), initialState(2));
  const r = restoreState({
    version: 1,
    stats: { fullness: 300 },
    prefs: { scale: -4 },
    log: null,
  });
  assert.equal(r.stats.fullness, 100);
  assert.equal(r.prefs.scale, 0.75);
  assert.equal(r.model.id, 'qwen');
});
test('food filters names and size, never content', () => {
  assert.equal(foodHint('test.TXT', 0), '');
  for (const [n, z] of [
    ['.notes.txt', 1],
    ['thing.exe', 0],
    ['a.txt', 101 * 1024 ** 2],
  ])
    assert.ok(foodHint(n, z));
});
test('roaming stays in the work area including negative monitor coordinates', () => {
  const a = { x: -1920, y: -80, width: 1920, height: 1000 },
    p = { x: -400, y: 700 };
  assert.deepEqual(confine(8000, -900, a, 200, 100), { x: -200, y: -80 });
  for (let i = 0; i < 120; i++) {
    const v = destination(p, a, 200, 100, true);
    assert.deepEqual(v, confine(v.x, v.y, a, 200, 100));
  }
  const target = destination(p, a, 200, 100, true, () => 0.2);
  assert.equal(target.y, 820);
  assert.deepEqual(advance({ x: 0, y: 0 }, { x: 1, y: 0 }, 80, 0.1), {
    x: 1,
    y: 0,
    arrived: true,
  });
});
test('transparent margin / offscreen / full pass-through / dragging hit policy', () => {
  const mask = new Uint8Array((MASK_W * MASK_H) / 8),
    i = 48 * MASK_W + 40;
  mask[i >> 3] |= 1 << (i & 7);
  const b = { x: -150, y: 7, width: 160, height: 192 },
    p = { x: -70, y: 103 };
  assert.equal(shouldIgnore(mask, b, p, false), false);
  assert.equal(shouldIgnore(mask, b, p, true), true);
  assert.equal(shouldIgnore(mask, b, { x: -151, y: 103 }, false), true);
  assert.equal(shouldIgnore(mask, b, { x: -149, y: 8 }, false), true);
  assert.equal(shouldIgnore(null, b, p, false), true);
  assert.equal(shouldIgnore(null, b, p, false, true), false);
  assert.equal(shouldIgnore(mask, b, p, true, true), true);
});

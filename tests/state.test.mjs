import test from 'node:test';
import assert from 'node:assert/strict';
import {
  initialState,
  reduceState,
  restoreState,
  foodHint,
} from '../core/state.mjs';
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
    key: 'scale',
    value: 1.35,
  });
  assert.equal(restoreState(s).prefs.scale, 1.35);
  assert.equal(
    restoreState({ ...s, prefs: { ...s.prefs, passThrough: true } }).prefs
      .passThrough,
    undefined,
  );
  assert.equal(
    reduceState(s, { type: 'pref', key: 'passThrough', value: true }),
    s,
  );
  assert.deepEqual(restoreState({}, 2), initialState(2));
  const r = restoreState({
    version: 1,
    stats: { fullness: 300 },
    prefs: { scale: -4 },
    log: null,
  });
  assert.equal(r.stats.fullness, 100);
  assert.equal(r.prefs.scale, 0.6);
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

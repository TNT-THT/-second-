import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  Wander,
  STILL,
  actorSize,
  resizeAnchored,
  contains,
  FRAME_ASPECT,
} from '../core/motion.mjs';
import { performanceFor, matchAnimation } from '../core/performance.mjs';
const area = { x: -800, y: -120, width: 800, height: 540 },
  size = actorSize(1, area);
test('one uniform scale for the native hit window and the image, including display fit', () => {
  for (const p of [0.6, 1, 1.25, 1.6]) {
    const s = actorSize(p, area);
    assert.ok(Math.abs(s.height - 218 * p) <= 0.5);
    assert.ok(Math.abs(s.width / s.height - FRAME_ASPECT) < 0.005);
  }
  const s = actorSize(1.6, { width: 50, height: 40 });
  assert.ok(s.width <= 50 && s.height <= 40);
  assert.equal(s.height, 40);
  const b = { x: -200, y: 20, width: 100, height: 120 };
  assert.equal(
    resizeAnchored(b, { width: 160, height: 190 }, area, 'floor').y + 190,
    140,
  );
  assert.equal(
    resizeAnchored(b, { width: 160, height: 190 }, area, 'right').x + 160,
    -100,
  );
  assert.equal(
    resizeAnchored(b, { width: 160, height: 190 }, area, 'top').y,
    20,
  );
});
test('whole compact hit rectangle: borders count, outside desktop does not', () => {
  const b = { x: -200, y: -50, ...size };
  for (const [x, y] of [
    [0, 0],
    [1, 1],
    [size.width - 1, size.height - 1],
  ])
    assert.ok(contains(b, { x: b.x + x, y: b.y + y }));
  assert.equal(contains(b, { x: b.x + size.width, y: b.y + 1 }), false);
});
test('actual route walks to a wall, climbs, glides at the top and descends, with no diagonal skating', () => {
  const w = new Wander(
      { x: area.x + 300, y: area.y + area.height - size.height },
      area,
      size,
      { wait: 0, rng: () => 0.3, surface: 'floor' },
    ),
    seen = new Set();
  let down = false;
  for (let i = 0; i < 2800; i++) {
    const old = { ...w.pos },
      m = w.step(0.033);
    const dx = w.pos.x - old.x,
      dy = w.pos.y - old.y;
    assert.ok(!(Math.abs(dx) > 0.001 && Math.abs(dy) > 0.001));
    assert.ok(
      w.pos.x >= w.l && w.pos.x <= w.r && w.pos.y >= w.t && w.pos.y <= w.b,
    );
    if (!m.moving) continue;
    seen.add(m.kind);
    if (m.kind === 'walk') assert.equal(m.face, Math.sign(dx));
    if (m.kind === 'climb') {
      assert.ok(w.pos.x === w.l || w.pos.x === w.r);
      assert.equal(dx, 0);
      if (dy > 0) down = true;
    }
    if (m.kind === 'glide') assert.equal(w.pos.y, w.t);
    assert.ok(m.phase >= 0 && m.phase < 1);
  }
  assert.deepEqual([...seen].sort(), ['climb', 'glide', 'walk']);
  assert.ok(down);
});
test('dropping close to either side or ceiling attaches there; stopping cannot advance a footprint', () => {
  const r = new Wander({ x: -401, y: 0 }, area, size);
  const p = r.place({ x: r.l + 3, y: 0 }, area, size, { snap: true, wait: 0 });
  assert.equal(p.x, r.l);
  assert.equal(r.track().face, -1);
  assert.equal(r.track().kind, 'climb');
  const m = r.step(0.06),
    after = { ...r.pos };
  for (let i = 0; i < 15; i++)
    assert.equal(r.step(0.09, { blocked: true }).phase, m.phase);
  assert.deepEqual(after, r.pos);
  r.place({ x: r.r - 2, y: 0 }, area, size, { snap: true });
  assert.equal(r.surface, 'right');
  r.place({ x: r.l + 80, y: r.t + 4 }, area, size, { snap: true });
  assert.equal(r.track().kind, 'glide');
  assert.equal(r.pos.y, r.t);
  r.step(0.03, { edges: false, roam: false });
  assert.equal(r.surface, 'floor');
});
test('stride phase depends on traveled distance at current scale, not elapsed animation time', () => {
  const w = new Wander({ x: -500, y: 110 }, area, size, {
    wait: 0,
    rng: () => 0.3,
    surface: 'floor',
  });
  const from = w.pos.x,
    m = w.step(0.04);
  assert.ok(
    Math.abs(
      m.phase -
        Math.abs(w.pos.x - from) / ((size.height * 4 * 87 * 0.39) / 480),
    ) < 1e-8,
  );
  assert.equal(w.step(0.05, { roam: false }).phase, m.phase);
});
test('file hover and cancel are not eating; pickup, successful meal and manual reactions override travel', () => {
  const v = { track: { ...STILL, kind: 'walk' } };
  assert.equal(performanceFor({ ...v, offer: true }), 'notice');
  assert.equal(performanceFor({ ...v, held: true }), 'held');
  assert.equal(performanceFor({ ...v, landing: true }), 'land');
  assert.equal(performanceFor({ ...v, reaction: { kind: 'eat' } }), 'eat');
  assert.equal(performanceFor(v), 'walk');
  assert.equal(performanceFor({ ...v, sleeping: true }), 'sleep');
  assert.equal(performanceFor({ ...v, hover: true }), 'notice');
});
test('imported GLB actions select direction-specific clips, named actions and an idle fallback', () => {
  const clips = [
    'Idle',
    'Walk_Left',
    'Walk-Right',
    'Eating',
    'Drag',
    'FanDance',
    'Climbing',
  ].map((name) => ({ name }));
  for (const [k, f, i] of [
    ['walk', -1, 1],
    ['walk', 1, 2],
    ['eat', 1, 3],
    ['held', 1, 4],
    ['play', 1, 5],
    ['climb', -1, 6],
    ['glide', 1, 0],
  ])
    assert.equal(matchAnimation(clips, k, f).index, i);
  assert.equal(matchAnimation([], 'idle').index, -1);
});
test('all authored performance atlases contain the advertised number of complete frames', () => {
  const manifest = JSON.parse(
    fs.readFileSync(new URL('../core/qwen-atlas.json', import.meta.url)),
  );
  assert.equal(Object.keys(manifest).length, 11);
  for (const [k, m] of Object.entries(manifest)) {
    const b = fs.readFileSync(
      new URL(`../public/art/motion/${k}.webp`, import.meta.url),
    );
    assert.equal(b.toString('ascii', 12, 16), 'VP8X');
    assert.equal(1 + b.readUIntLE(24, 3), m.width * m.cols);
    assert.equal(
      1 + b.readUIntLE(27, 3),
      m.height * Math.ceil(m.frames / m.cols),
    );
    assert.ok(m.frames >= 12 && m.seconds > 0);
  }
});

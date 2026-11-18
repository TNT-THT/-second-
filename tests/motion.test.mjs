import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  Wander,
  actorSize,
  contains,
  resizeAnchored,
  FRAME_ASPECT,
  PET_HEIGHT,
  STRIDE,
  platformSpan,
} from '../core/motion.mjs';
import { performanceFor, matchAnimation } from '../core/performance.mjs';
import {
  BUILTINS,
  clips,
  canFly,
  hasFlightClip,
  motionOptions,
} from '../core/repertoire.mjs';
import { initialState, reduceState, restoreState } from '../core/state.mjs';
import { drawChibi, poseAt } from '../art-src/chibi/puppet.mjs';
const area = { x: -800, y: 0, width: 800, height: 650 },
  size = actorSize(1, area),
  bottom = area.height - size.height,
  L = { id: 'foreground', x: -665, y: 390, width: 410 };
const make = (x = -532, y = bottom, opts = {}) =>
  new Wander({ x, y }, area, size, { wait: 0, rng: () => 0.3, ...opts });
const frames = (w, n, o = {}) =>
  Array.from({ length: n }, () => w.step(1 / 30, o));
test('proportional live hit rectangle fits work area, anchors feet and counts the transparent corner', () => {
  for (const scale of [0.6, 1, 1.35, 1.6]) {
    const s = actorSize(scale, area);
    assert.ok(Math.abs(s.height - PET_HEIGHT * scale) <= 0.5);
    assert.ok(Math.abs(s.width / s.height - FRAME_ASPECT) < 0.006);
  }
  assert.equal(actorSize(1.6, { width: 40, height: 30 }).height, 30);
  assert.ok(contains({ x: -100, y: 20, ...size }, { x: -100, y: 20 }));
  assert.ok(!contains({ x: -100, y: 20, ...size }, { x: -101, y: 20 }));
  assert.equal(
    resizeAnchored(
      { x: -550, y: 25, width: 100, height: 150 },
      size,
      area,
      'window',
    ).y + size.height,
    175,
  );
});
test('air release falls while hovered, sleeping or parked; no landing before contact or tunneling', () => {
  const w = make(-530, 90);
  assert.equal(w.track().kind, 'fall');
  let prev = w.pos.y,
    land = 0;
  for (let i = 0; i < 60; i++) {
    const st = w.step(0.04, { roam: false, blocked: true });
    assert.ok(w.pos.y >= prev && w.pos.y <= bottom);
    if (st.kind === 'land') {
      assert.equal(w.pos.y, bottom);
      land++;
    }
    prev = w.pos.y;
  }
  assert.ok(land >= 6);
  assert.equal(w.surface, 'floor');
  assert.equal(w.track().kind, 'idle');
});
test('one-way visible window top catches feet, moves with its host, releases on removal', () => {
  const w = make(-520, 50);
  const at = frames(w, 40, { ledge: L, roam: false });
  assert.ok(at.some((t) => t.kind === 'land' && t.surface === 'window'));
  assert.equal(w.pos.y + size.height, L.y);
  const old = w.pos.x;
  const L2 = { ...L, x: L.x - 30, y: L.y + 13 };
  w.step(0.03, { ledge: L2, blocked: true });
  assert.equal(w.pos.x, old - 30);
  assert.equal(w.pos.y + size.height, L2.y);
  assert.ok(
    frames(w, 150, { ledge: L2 }).some(
      (t) => t.kind === 'walk' && t.surface === 'window',
    ),
  );
  const span = platformSpan(L2, area, size);
  assert.ok(w.pos.x >= span.l && w.pos.x <= span.r);
  assert.equal(w.step(0.033, { roam: false }).kind, 'fall');
  frames(w, 90);
  assert.equal(w.surface, 'floor');
});
test('does not teleport up onto a window above the feet or outside its horizontal span', () => {
  for (const [x, y] of [
    [-770, 260],
    [-180, 5],
    [-510, 240],
  ]) {
    const w = make(x, y);
    frames(w, 90, { ledge: L, roam: false });
    assert.equal(w.pos.y, bottom);
    assert.equal(w.surface, 'floor');
  }
  assert.equal(platformSpan({ ...L, y: 10 }, area, size), null);
});
test('ground mode walks both directions, climbs and traverses the ceiling with a grip, never flies', () => {
  for (const [rnd, dir] of [
    [0.3, 1],
    [0.1, -1],
  ]) {
    const w = make(-531, bottom, { rng: () => rnd });
    const first = w.step(0.04);
    assert.equal(first.face, dir);
    assert.equal(first.kind, 'walk');
    assert.ok(
      Math.abs(first.phase - Math.abs(w.pos.x + 531) / (size.height * STRIDE)) <
        1e-7,
    );
    const seen = new Set();
    for (let i = 0; i < 4000; i++) {
      const old = { ...w.pos },
        t = w.step(0.042);
      seen.add(t.kind);
      assert.ok(
        !(Math.abs(old.y - w.pos.y) > 0.01 && Math.abs(old.x - w.pos.x) > 0.01),
      );
      if (t.kind === 'hang') assert.equal(w.pos.y, 0);
    }
    assert.ok(seen.has('climb') && seen.has('hang') && !seen.has('fly'));
  }
});
test('release can latch to an allowed edge; denying the edge forces descent, not a stationary walk', () => {
  const w = make();
  w.place({ x: w.r - 3, y: 33 }, area, size, { snap: true, release: true });
  assert.equal(w.surface, 'right');
  assert.equal(
    w.step(0.033, { disabled: ['climb'], roam: false }).kind,
    'fall',
  );
  w.place({ x: w.l + 90, y: 3 }, area, size, { snap: true });
  assert.equal(w.track().kind, 'hang');
  assert.equal(w.step(0.03, { edges: false, blocked: true }).kind, 'fall');
});
test('only explicit flight can hold an arbitrary point in air; two-axis roam, disable even during hover', () => {
  const w = make();
  w.place({ x: -510, y: 128 }, area, size, {
    flight: true,
    release: true,
    wait: 0,
  });
  frames(w, 20, { roam: false, flight: true });
  assert.deepEqual(w.pos, { x: -510, y: 128 });
  assert.equal(w.track().kind, 'fly');
  frames(w, 10, { flight: true });
  assert.ok(w.pos.x !== -510 && w.pos.y !== 128);
  const st = w.step(0.035, { flight: true, disabled: ['fly'], blocked: true });
  assert.equal(st.kind, 'fall');
  frames(w, 120, { flight: true, disabled: ['fly'], roam: false });
  assert.equal(w.surface, 'floor');
});
test('stationary leisure respects preferred activity, quiet, and in-progress revocation', () => {
  for (const k of ['read', 'tea', 'play']) {
    const w = make();
    assert.ok(
      frames(w, 150, { roam: false, leisure: k }).some((t) => t.kind === k),
    );
    assert.notEqual(
      w.step(0.04, { leisure: k, roam: false, disabled: [k] }).kind,
      k,
    );
    assert.ok(
      frames(w, 600, { leisure: 'quiet', roam: false }).every(
        (t) => t.kind === 'idle',
      ),
    );
  }
});
test('gravity/flight poses have priority, disabling a reaction does not undo the event', () => {
  for (const k of ['fall', 'land', 'fly', 'climb', 'hang']) {
    assert.equal(
      performanceFor({
        track: { kind: k },
        hover: true,
        reaction: { kind: 'pat' },
      }),
      k,
    );
  }
  const v = { track: { kind: 'walk' } };
  assert.equal(performanceFor({ ...v, held: true }), 'held');
  assert.equal(performanceFor({ ...v, offer: true }), 'notice');
  assert.equal(
    performanceFor({
      ...v,
      reaction: { kind: 'eat' },
      prefs: { disabledClips: ['eat'] },
    }),
    'idle',
  );
});
test('GLB flight is a real named clip, not the idle fallback; disabled clips never used as fallback', () => {
  assert.ok(hasFlightClip([{ name: 'Wing_FlightLoop' }]));
  assert.ok(!hasFlightClip([{ name: 'ButterflyDance' }]));
  const cs = ['Float', 'Idle', 'Walk_Left', 'WalkRight', 'Read'].map(
    (name) => ({ name }),
  );
  assert.equal(matchAnimation(cs, 'fly').index, 0);
  assert.equal(matchAnimation(cs, 'walk', -1).index, 2);
  assert.equal(matchAnimation(cs, 'walk', 1).index, 3);
  assert.equal(matchAnimation(cs, 'read', 1, ['read']).index, 1);
  assert.equal(matchAnimation([cs[0]], 'idle', 1, ['fly']).index, -1);
  assert.equal(matchAnimation([cs[1]], 'fly').index, -1);
  assert.ok(!canFly({ kind: 'image', flyClip: true }));
});
test('all 15 actions share an articulated rig; both built-in cards and portraits are exact exports, not old bitmaps', () => {
  assert.equal(Object.keys(clips).length, 15);
  for (const { id } of BUILTINS) {
    for (const portrait of [false, true]) {
      const svg = fs.readFileSync(
        new URL(
          `../public/art/${id}${portrait ? '-face' : ''}.svg`,
          import.meta.url,
        ),
        'utf8',
      );
      assert.equal(svg.trim(), drawChibi({ skin: id, portrait }));
      assert.ok(!/<image|<script/.test(svg));
    }
    for (const k of Object.keys(clips)) {
      const a = poseAt(k, 0.15),
        b = poseAt(k, 0.4, -1);
      assert.ok(!JSON.stringify(a).includes('NaN'));
      assert.notDeepEqual({ ...a, mirror: '' }, { ...b, mirror: '' });
    }
  }
});
test('model/permission changes turn off the actual flight mode and migrate prior preferences', () => {
  const act = (s, type, rest) => reduceState(s, { type, ...rest });
  let s = initialState();
  s = act(s, 'pref', { key: 'flight', value: true });
  assert.equal(motionOptions(s).flight, true);
  s = act(s, 'animation', { key: 'fly', enabled: false });
  assert.equal(motionOptions(s).flight, false);
  assert.equal(act(s, 'pref', { key: 'flight', value: true }), s);
  s = restoreState(s);
  assert.ok(s.prefs.disabledClips.includes('fly'));
  s = act(s, 'animation', { key: 'fly', enabled: true });
  assert.equal(s.prefs.flight, false);
  s = act(s, 'pref', { key: 'flight', value: true });
  s = act(s, 'select', {
    model: { id: 'local-image', name: '小花', kind: 'image' },
  });
  assert.equal(s.prefs.flight, false);
  assert.equal(act(s, 'animation', { key: 'fall', enabled: false }), s);
  let t = restoreState({
    version: 1,
    model: { id: 'qwen', name: '旧版', kind: 'illustration' },
    prefs: { scale: 1.4 },
  });
  assert.equal(t.model.edition, '花庭蓝');
  assert.equal(t.prefs.scale, 1.4);
  assert.equal(t.prefs.flight, false);
});
test('authored phrases cycle separately, survive restart, meals still count when eating animation is muted', () => {
  let s = initialState(),
    lines = new Set();
  for (let i = 0; i < 6; i++) {
    s = reduceState(s, { type: 'chat' });
    lines.add(s.reaction.text);
    s = restoreState(reduceState(s, { type: 'pat' }));
  }
  assert.equal(lines.size, 6);
  s = reduceState(s, { type: 'animation', key: 'eat', enabled: false });
  assert.equal(reduceState(s, { type: 'meal' }).meals, 1);
});

// All geometry uses logical pixels (DIPs). Both the small native window and the studio
// sample the SAME choreography, including gait phase, rather than inferring a pose from a timer.
export const FRAME_ASPECT = 352 / 480,
  MIN_SCALE = 0.6,
  MAX_SCALE = 1.6,
  PET_HEIGHT = 218;
export const clamp = (n, lo, hi) => Math.max(lo, Math.min(Math.max(lo, hi), n));
export function actorSize(scale, area, base = PET_HEIGHT, reserve = 0) {
  const h = Math.max(
    1,
    Math.min(
      base * clamp(Number.isFinite(scale) ? scale : 1, MIN_SCALE, MAX_SCALE),
      area.height - reserve,
      area.width / FRAME_ASPECT,
    ),
  );
  return {
    width: Math.max(1, Math.round(h * FRAME_ASPECT)),
    height: Math.max(1, Math.round(h)),
  };
}
export function confine(x, y, a, w, h) {
  return {
    x: Math.round(clamp(x, a.x, a.x + a.width - w)),
    y: Math.round(clamp(y, a.y, a.y + a.height - h)),
  };
}
export function contains(b, p) {
  return (
    p.x >= b.x && p.x < b.x + b.width && p.y >= b.y && p.y < b.y + b.height
  );
}
export function resizeAnchored(b, size, a, surface) {
  const x =
    surface === 'right'
      ? b.x + b.width - size.width
      : surface === 'left'
        ? b.x
        : b.x + (b.width - size.width) / 2;
  const y =
    surface === 'top'
      ? b.y
      : ['left', 'right'].includes(surface)
        ? b.y + (b.height - size.height) / 2
        : b.y + b.height - size.height;
  return { ...confine(x, y, a, size.width, size.height), ...size };
}
const wrap = (n) => ((n % 1) + 1) % 1;
export const STILL = Object.freeze({
  kind: 'idle',
  face: 1,
  phase: 0,
  moving: false,
  surface: 'floor',
});
export class Wander {
  constructor(pos, area, size, { wait = 2, rng = Math.random, surface } = {}) {
    this.rng = rng;
    this.face = 1;
    this.phase = 0;
    this.vertical = -1;
    this.place(pos, area, size, { wait, surface });
  }
  place(pos, area, size, { wait = 1, surface, snap = false } = {}) {
    this.area = area;
    this.size = size;
    this.l = area.x;
    this.t = area.y;
    this.r = Math.max(this.l, this.l + area.width - size.width);
    this.b = Math.max(this.t, this.t + area.height - size.height);
    const p = confine(pos.x, pos.y, area, size.width, size.height),
      slop = snap ? Math.min(14, size.height * 0.08) : 0.5;
    this.surface =
      surface ||
      (p.y <= this.t + slop && this.b > this.t
        ? 'top'
        : p.x <= this.l + slop
          ? 'left'
          : p.x >= this.r - slop
            ? 'right'
            : 'floor');
    if (this.surface === 'left') p.x = this.l;
    if (this.surface === 'right') p.x = this.r;
    if (this.surface === 'top') p.y = this.t;
    if (snap && this.surface === 'floor' && p.y >= this.b - slop) p.y = this.b;
    this.pos = p;
    this.goal = null;
    this.rest = wait;
    this.autoPlay = 0;
    this.transition = null;
    this.phase = 0;
    if (this.surface === 'floor' && Math.abs(p.x - this.r) < 1) this.face = -1;
    else if (this.surface === 'floor' && Math.abs(p.x - this.l) < 1)
      this.face = 1;
    if (this.surface === 'left' || this.surface === 'right')
      this.vertical = p.y <= this.t + 0.5 ? 1 : -1;
    return this.pos;
  }
  track(moving = false, playing = false) {
    const wall = this.surface === 'left' || this.surface === 'right';
    return {
      kind: wall
        ? 'climb'
        : this.surface === 'top'
          ? 'glide'
          : moving
            ? 'walk'
            : playing
              ? 'play'
              : 'idle',
      face: wall ? (this.surface === 'left' ? -1 : 1) : this.face,
      phase: this.phase,
      moving,
      surface: this.surface,
    };
  }
  stop(wait = 0.6) {
    this.rest = wait;
    this.goal = null;
    this.autoPlay = 0;
  }
  choose(edges) {
    const p = this.pos,
      r = this.rng,
      h = this.size.height;
    this.transition = null;
    if (!edges) this.surface = 'floor';
    if (this.surface === 'left' || this.surface === 'right') {
      const end = this.vertical < 0 ? this.t : this.b;
      const dist = Math.min(Math.abs(end - p.y), h * (0.75 + r() * 1.25));
      this.goal = { x: p.x, y: p.y + this.vertical * dist };
      if (Math.abs(this.goal.y - end) < 0.5)
        this.transition = this.vertical < 0 ? 'top' : 'floor';
    } else {
      if (p.x <= this.l + 0.5) this.face = 1;
      else if (p.x >= this.r - 0.5) this.face = -1;
      else if (this.surface === 'floor' && r() < 0.16) this.face *= -1;
      const end = this.face > 0 ? this.r : this.l;
      const patrol = this.surface === 'top' || (edges && r() < 0.65);
      const dist = patrol
        ? Math.abs(end - p.x)
        : Math.min(Math.abs(end - p.x), h * (0.42 + r() * 1.1));
      this.goal = { x: p.x + this.face * dist, y: p.y };
      if (edges && Math.abs(this.goal.x - end) < 0.5)
        this.transition = this.face > 0 ? 'right' : 'left';
    }
  }
  step(seconds, { roam = true, edges = true, blocked = false } = {}) {
    const dt = clamp(seconds, 0, 0.1);
    if (!edges && this.transition) this.goal = this.transition = null;
    if (!edges && this.surface !== 'floor') {
      this.surface = 'floor';
      this.stop();
    }
    if (!roam || blocked) {
      this.autoPlay = 0;
      return this.track();
    }
    if (this.rest > 0) {
      this.rest = Math.max(0, this.rest - dt);
      this.autoPlay = Math.max(0, this.autoPlay - dt);
      return this.track(false, this.autoPlay > 0);
    }
    if (!this.goal) this.choose(edges);
    const dx = this.goal.x - this.pos.x,
      dy = this.goal.y - this.pos.y,
      len = Math.hypot(dx, dy),
      wall = this.surface === 'left' || this.surface === 'right';
    const h = this.size.height,
      speed = h * (wall ? 0.235 : this.surface === 'top' ? 0.41 : 0.31),
      step = Math.min(len, speed * dt);
    // One two-step cycle moves precisely four half-strides of the baked IK rig.
    const stride =
      h *
      (wall ? 0.172 : this.surface === 'top' ? 0.68 : (4 * 87 * 0.39) / 480);
    this.phase = wrap(this.phase + ((wall && dy > 0 ? -1 : 1) * step) / stride);
    if (len > 0) {
      this.pos = {
        x: this.pos.x + (dx * step) / len,
        y: this.pos.y + (dy * step) / len,
      };
    }
    const pose = this.track(step > 0);
    if (len - step < 0.001) {
      const before = this.surface;
      this.pos = { ...this.goal };
      this.goal = null;
      if (this.transition) {
        this.surface = this.transition;
        this.transition = null;
        if (this.surface === 'left' || this.surface === 'right')
          this.vertical = before === 'top' ? 1 : -1;
        else this.face = before === 'left' ? 1 : -1;
        this.rest = 0.3;
        this.phase = 0;
        if (this.surface === 'floor' && this.rng() < 0.55) {
          this.rest = 1.75;
          this.autoPlay = 1.55;
        }
      } else {
        this.rest = 1.3 + this.rng() * 2;
        this.autoPlay =
          this.surface === 'floor' && this.rng() < 0.48
            ? Math.min(this.rest, 1.55)
            : 0;
      }
    }
    return pose;
  }
}

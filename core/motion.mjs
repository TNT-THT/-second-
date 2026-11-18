// All geometry uses logical pixels (DIPs). Both the small native window and the studio
// sample the SAME choreography, including gait phase, rather than inferring a pose from a timer.
export const FRAME_ASPECT = 232 / 252,
  MIN_SCALE = 0.6,
  MAX_SCALE = 1.6,
  PET_HEIGHT = 174;
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
export const STRIDE = 52 / 252,
  LAND_TIME = 0.43;
// A platform is geometry only, never a screenshot/title. Its top must be high enough above
// the taskbar AND low enough for the complete pet to stand on it without leaving the display.
export function platformSpan(ledge, a, size) {
  if (
    !ledge ||
    !Number.isFinite(ledge.x) ||
    !Number.isFinite(ledge.y) ||
    !Number.isFinite(ledge.width) ||
    ledge.width < 30
  )
    return null;
  const y = ledge.y - size.height,
    mid = size.width * 0.5,
    pad = Math.min(15, ledge.width * 0.12);
  if (y < a.y - 0.1 || y >= a.y + a.height - size.height - 1) return null;
  const l = Math.max(a.x, ledge.x + pad - mid),
    r = Math.min(a.x + a.width - size.width, ledge.x + ledge.width - pad - mid);
  return r - l > 2 ? { l, r, y, id: ledge.id, x: ledge.x } : null;
}
export class Wander {
  constructor(pos, area, size, opts = {}) {
    this.rng = opts.rng || Math.random;
    this.face = 1;
    this.vertical = -1;
    this.phase = 0;
    this.place(pos, area, size, opts);
  }
  place(
    pos,
    area,
    size,
    {
      wait = 1,
      surface,
      snap = false,
      flight = false,
      ledge,
      edges = true,
      disabled = [],
      release = false,
    } = {},
  ) {
    this.area = area;
    this.size = { width: size.width, height: size.height };
    this.l = area.x;
    this.t = area.y;
    this.r = Math.max(this.l, area.x + area.width - size.width);
    this.b = Math.max(this.t, area.y + area.height - size.height);
    const p = confine(pos.x, pos.y, area, size.width, size.height),
      slop = snap ? Math.min(12, size.height * 0.065) : 0.6;
    const span = platformSpan(ledge, area, size),
      near =
        span &&
        p.x >= span.l &&
        p.x <= span.r &&
        Math.abs(p.y - span.y) <= slop;
    this.pos = p;
    this.phase = 0;
    this.vy = 0;
    this.landLeft = 0;
    this.takeoff = null;
    this.support = null;
    this.stop(wait);
    this.quietTime = 0;
    this.surface = flight
      ? 'air'
      : surface ||
        (p.y >= this.b - slop
          ? 'floor'
          : near
            ? 'window'
            : edges && !disabled.includes('hang') && p.y <= this.t + slop
              ? 'top'
              : edges && !disabled.includes('climb') && p.x <= this.l + slop
                ? 'left'
                : edges && !disabled.includes('climb') && p.x >= this.r - slop
                  ? 'right'
                  : 'fall');
    if (this.surface === 'floor') p.y = this.b;
    if (this.surface === 'window') {
      if (span) {
        p.y = span.y;
        p.x = clamp(p.x, span.l, span.r);
        this.support = span;
      } else this.surface = 'fall';
    }
    if (this.surface === 'left' || this.surface === 'right') {
      p.x = this.surface === 'left' ? this.l : this.r;
      this.vertical = p.y <= this.t + 0.5 ? 1 : -1;
    }
    if (this.surface === 'top') p.y = this.t;
    if (release && ['floor', 'window'].includes(this.surface))
      this.landLeft = LAND_TIME;
    return this.pos;
  }
  stop(wait = 0.65) {
    this.rest = wait;
    this.goal = null;
    this.turn = null;
    this.activity = null;
    this.funLeft = 0;
  }
  track(moving = false) {
    const wall = ['left', 'right'].includes(this.surface),
      kind =
        this.surface === 'fall'
          ? 'fall'
          : this.surface === 'air'
            ? 'fly'
            : wall
              ? 'climb'
              : this.surface === 'top'
                ? 'hang'
                : this.landLeft > 0
                  ? 'land'
                  : moving
                    ? 'walk'
                    : this.activity || 'idle';
    return {
      kind,
      surface: this.surface,
      moving,
      face: wall ? (this.surface === 'right' ? 1 : -1) : this.face,
      phase:
        kind === 'land'
          ? Math.min(0.999, 1 - this.landLeft / LAND_TIME)
          : this.phase,
    };
  }
  fall() {
    this.surface = 'fall';
    this.vy = 0;
    this.landLeft = 0;
    this.takeoff = null;
    this.support = null;
    this.stop(0.8);
  }
  chooseLeisure(pool) {
    if (!pool.length) return;
    this.activity =
      pool[Math.min(pool.length - 1, Math.floor(this.rng() * pool.length))];
    this.funLeft = 3.3 + this.rng() * 1.5;
    this.quietTime = 0;
  }
  step(
    seconds,
    {
      roam = true,
      edges = true,
      blocked = false,
      flight = false,
      ledge = null,
      disabled = [],
      leisure = 'mix',
    } = {},
  ) {
    const dt = clamp(seconds, 0, 0.1),
      p = this.pos,
      h = this.size.height,
      span = platformSpan(ledge, this.area, this.size),
      climbing = edges && !disabled.includes('climb'),
      hanging = edges && !disabled.includes('hang');
    flight = flight && !disabled.includes('fly');
    if (this.surface === 'window') {
      if (!span || span.id !== this.support?.id) this.fall();
      else {
        if (span.x !== this.support.x || span.r !== this.support.r)
          this.goal = null;
        p.x = clamp(p.x + span.x - this.support.x, span.l, span.r);
        p.y = span.y;
        this.support = span;
      }
    }
    if (flight && this.surface !== 'air') {
      const onGround = ['window', 'floor'].includes(this.surface);
      this.surface = 'air';
      this.support = null;
      this.landLeft = 0;
      this.stop(0.25);
      this.takeoff = onGround ? Math.max(this.t, p.y - h * 0.48) : null;
    }
    if (
      (!flight && this.surface === 'air') ||
      (!climbing && ['left', 'right'].includes(this.surface)) ||
      (!hanging && this.surface === 'top')
    )
      this.fall();
    // Gravity and impact ALWAYS run, even with roaming off, a mouse over the pet, or a reaction.
    if (this.surface === 'fall') {
      const platform =
          span && p.x >= span.l && p.x <= span.r && p.y <= span.y + 0.01
            ? span
            : null,
        target = platform?.y ?? this.b;
      this.vy = Math.min(this.vy + dt * Math.max(660, h * 7.2), h * 5.5);
      p.y = Math.min(target, p.y + this.vy * dt);
      if (p.y >= target - 0.001) {
        p.y = target;
        this.surface = platform ? 'window' : 'floor';
        this.support = platform;
        this.vy = 0;
        this.landLeft = LAND_TIME;
        this.quietTime = 0;
        this.stop(1.1);
        return this.track();
      }
      return this.track(true);
    }
    if (this.landLeft > 0) {
      this.landLeft = Math.max(0, this.landLeft - dt);
      return this.track();
    }
    if (this.surface === 'air') {
      if (this.takeoff !== null) {
        p.y = Math.max(this.takeoff, p.y - h * 0.66 * dt);
        if (p.y <= this.takeoff + 0.05) this.takeoff = null;
        return this.track(true);
      }
      if (!roam || blocked) return this.track();
      if (this.rest > 0) {
        this.rest -= dt;
        return this.track();
      }
      if (!this.goal) {
        let x = this.l + (this.r - this.l) * this.rng(),
          y = this.t + (this.b - this.t) * this.rng();
        if (Math.hypot(x - p.x, y - p.y) < 20) {
          x = p.x < (this.l + this.r) / 2 ? this.r : this.l;
          y = p.y < (this.t + this.b) / 2 ? this.b : this.t;
        }
        this.goal = { x, y };
      }
      const dx = this.goal.x - p.x,
        dy = this.goal.y - p.y,
        len = Math.hypot(dx, dy),
        n = Math.min(len, h * 0.7 * dt);
      if (Math.abs(dx) > 1) this.face = Math.sign(dx);
      if (len) {
        p.x += (dx * n) / len;
        p.y += (dy * n) / len;
      }
      if (len - n < 0.01) {
        this.goal = null;
        this.rest = 0.55 + this.rng() * 1.15;
      }
      return this.track(n > 0);
    }
    const ground = ['floor', 'window'].includes(this.surface),
      pool = ['play', 'read', 'tea'].filter(
        (k) => !disabled.includes(k) && (leisure === 'mix' || leisure === k),
      );
    if (this.activity && !pool.includes(this.activity)) this.activity = null;
    if (blocked) {
      this.activity = null;
      this.quietTime = 0;
      return this.track();
    }
    if (this.activity) {
      this.funLeft -= dt;
      if (this.funLeft <= 0) {
        this.activity = null;
        this.quietTime = 0;
        this.rest = 0.6;
      }
      return this.track();
    }
    if (!roam) {
      if (ground && (this.quietTime += dt) > 3.6) this.chooseLeisure(pool);
      return this.track();
    }
    if (this.rest > 0) {
      this.rest -= dt;
      return this.track();
    }
    if (!this.goal) {
      const wall = this.surface === 'left' || this.surface === 'right';
      this.turn = null;
      if (wall) {
        const end = this.vertical < 0 ? this.t : this.b;
        this.goal = { x: p.x, y: end };
        this.turn = this.vertical < 0 ? (hanging ? 'top' : 'reverse') : 'floor';
      } else {
        const l = this.surface === 'window' ? span.l : this.l,
          r = this.surface === 'window' ? span.r : this.r;
        if (p.x <= l + 0.51) this.face = 1;
        else if (p.x >= r - 0.51) this.face = -1;
        else if (ground && this.rng() < 0.14) this.face *= -1;
        const end = this.face === 1 ? r : l,
          dist = Math.abs(end - p.x),
          travel =
            this.surface === 'top' || this.rng() < 0.62
              ? dist
              : Math.min(dist, h * (0.5 + this.rng()));
        this.goal = { x: p.x + this.face * travel, y: p.y };
        if (this.surface !== 'window' && climbing && travel >= dist - 0.01)
          this.turn = this.face > 0 ? 'right' : 'left';
      }
    }
    const dx = this.goal.x - p.x,
      dy = this.goal.y - p.y,
      len = Math.hypot(dx, dy),
      wall = this.surface === 'left' || this.surface === 'right',
      n = Math.min(len, h * (wall ? 0.29 : 0.36) * dt);
    if (len) {
      p.x += (dx * n) / len;
      p.y += (dy * n) / len;
    }
    this.phase = wrap(
      this.phase + ((wall && dy > 0 ? -1 : 1) * n) / (h * STRIDE),
    );
    const moving = this.track(n > 0.001);
    if (len - n < 0.001) {
      this.pos = { ...this.goal };
      this.goal = null;
      const prev = this.surface;
      if (this.turn === 'reverse') this.vertical *= -1;
      else if (
        this.turn &&
        (!['left', 'right'].includes(this.turn) || climbing) &&
        (this.turn !== 'top' || hanging)
      ) {
        this.surface = this.turn;
        if (this.surface === 'left' || this.surface === 'right')
          this.vertical = prev === 'top' ? 1 : -1;
        else this.face = prev === 'left' ? 1 : -1;
      }
      this.turn = null;
      this.rest =
        this.surface === 'top' || ['left', 'right'].includes(this.surface)
          ? 0.25
          : 1.2 + this.rng();
      if (['floor', 'window'].includes(this.surface) && this.rng() < 0.68)
        this.chooseLeisure(pool);
    }
    return moving;
  }
}

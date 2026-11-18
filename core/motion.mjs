const limit = (n, lo, hi) =>
  Math.max(lo, Math.min(Math.max(lo, hi), Math.round(n)));
export function confine(x, y, area, w, h) {
  return {
    x: limit(x, area.x, area.x + area.width - w),
    y: limit(y, area.y, area.y + area.height - h),
  };
}
// Positions are logical screen coordinates (DIPs); monitors may have negative origins.
export function destination(pos, area, w, h, edges, rng = Math.random) {
  const left = area.x,
    right = left + Math.max(0, area.width - w),
    top = area.y,
    bottom = top + Math.max(0, area.height - h);
  let x = left + rng() * (right - left),
    y = top + rng() * (bottom - top);
  if (edges && rng() < 0.8) {
    const walls = [
      { d: Math.abs(pos.y - bottom), side: 'b' },
      { d: Math.abs(pos.x - right), side: 'r' },
      { d: Math.abs(pos.x - left), side: 'l' },
      { d: Math.abs(pos.y - top), side: 't' },
    ].sort((a, b) => a.d - b.d);
    const side =
      rng() < 0.8 ? walls[0].side : walls[Math.floor(rng() * 4)].side;
    if (side === 'b') y = bottom;
    if (side === 't') y = top;
    if (side === 'l') x = left;
    if (side === 'r') x = right;
  }
  return confine(x, y, area, w, h);
}
export function advance(pos, target, speed, dt) {
  const dx = target.x - pos.x,
    dy = target.y - pos.y,
    d = Math.hypot(dx, dy),
    step = speed * Math.min(0.1, Math.max(0, dt));
  return !d || d <= step
    ? { ...target, arrived: true }
    : {
        x: pos.x + (dx / d) * step,
        y: pos.y + (dy / d) * step,
        arrived: false,
      };
}

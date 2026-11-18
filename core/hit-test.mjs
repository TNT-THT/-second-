// A small alpha map in window coordinates. Only character pixels are interactive;
// bubbles, ground shadows and the transparent margin don't intercept desktop clicks.
export const MASK_W = 80,
  MASK_H = 96;
export function shouldIgnore(
  mask,
  bounds,
  cursor,
  passThrough,
  dragging = false,
) {
  if (passThrough) return true;
  if (dragging) return false;
  const x = Math.floor(((cursor.x - bounds.x) * MASK_W) / bounds.width),
    y = Math.floor(((cursor.y - bounds.y) * MASK_H) / bounds.height);
  if (!mask || x < 0 || y < 0 || x >= MASK_W || y >= MASK_H) return true;
  const i = y * MASK_W + x;
  return !(mask[i >> 3] & (1 << (i & 7)));
}

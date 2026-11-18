import { MASK_W as w, MASK_H as h } from '../core/hit-test.mjs';
export function watchAlpha(root, send) {
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  let previous = '',
    stopped = false;
  function sample() {
    if (stopped || !ctx || !root) return;
    ctx.clearRect(0, 0, w, h);
    const el = root.querySelector('.pet-art .avatar');
    if (
      el &&
      (el instanceof HTMLCanvasElement ||
        (el instanceof HTMLImageElement && el.complete && el.naturalWidth))
    ) {
      const r = el.getBoundingClientRect(),
        iw = el.naturalWidth || el.width,
        ih = el.naturalHeight || el.height;
      const s = Math.min(r.width / iw, r.height / ih),
        dw = iw * s,
        dh = ih * s;
      try {
        ctx.drawImage(
          el,
          ((r.x + (r.width - dw) / 2) * w) / innerWidth,
          ((r.y + (r.height - dh) / 2) * h) / innerHeight,
          (dw * w) / innerWidth,
          (dh * h) / innerHeight,
        );
      } catch {
        /* Not ready yet. */
      }
    }
    const pixels = ctx.getImageData(0, 0, w, h).data,
      bytes = new Uint8Array((w * h) / 8);
    for (let i = 0; i < w * h; i++)
      if (pixels[4 * i + 3] >= 56) bytes[i >> 3] |= 1 << (i & 7);
    const mask = btoa(String.fromCharCode(...bytes));
    if (mask !== previous) {
      previous = mask;
      send(mask);
    }
  }
  sample();
  const timer = setInterval(sample, 150);
  return () => {
    stopped = true;
    clearInterval(timer);
  };
}

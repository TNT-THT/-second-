#pragma once
// Screen-adaptive placement for every speech bubble. Pure geometry, in virtual-screen coordinates.
#include <algorithm>
namespace pet {
struct RectI {
  int l = 0, t = 0, r = 0, b = 0;
  int width() const {
    return r - l;
  }
  int height() const {
    return b - t;
  }
  bool empty() const {
    return r <= l || b <= t;
  }
};
struct BubbleBox {
  RectI box;
  int tipX = 0;   // arrow tip on the box's horizontal edge
  bool below = false; // box sits below the anchor, arrow points up
};
// Prefer above the anchor, fall back below, then clamp entirely inside the work area.
inline BubbleBox placeBubble(RectI work, int anchorX, int anchorY, int w, int h, int gap = 8, int margin = 6) {
  BubbleBox out;
  if (!work.empty()) {
    const int maxW = std::max(1, work.width() - 2 * margin), maxH = std::max(1, work.height() - 2 * margin);
    w = std::clamp(w, 1, maxW);
    h = std::clamp(h, 1, maxH);
  }
  const int aboveTop = anchorY - gap - h, belowTop = anchorY + gap;
  if (!work.empty() && aboveTop >= work.t + margin)
    out.below = false;
  else if (!work.empty() && belowTop + h <= work.b - margin)
    out.below = true;
  else
    out.below = anchorY - work.t >= work.b - anchorY; // more room on whichever side
  int top = out.below ? belowTop : anchorY - gap - h;
  int left = anchorX - w / 2;
  if (!work.empty()) {
    top = std::clamp(top, work.t + margin, std::max(work.t + margin, work.b - margin - h));
    left = std::clamp(left, work.l + margin, std::max(work.l + margin, work.r - margin - w));
  }
  out.box = {left, top, left + w, top + h};
  const int pad = std::min(18, w / 3);
  out.tipX = std::clamp(anchorX, out.box.l + pad, std::max(out.box.l + pad, out.box.r - pad));
  return out;
}
} // namespace pet

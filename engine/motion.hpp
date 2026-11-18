#pragma once
#include "rules.hpp"
#include <algorithm>
#include <cmath>
#include <random>
namespace pet {
constexpr double pi = 3.141592653589793;
struct V {
  double x = 0, y = 0;
  V operator+(V b) const {
    return {x + b.x, y + b.y};
  }
  V operator-(V b) const {
    return {x - b.x, y - b.y};
  }
  V operator*(double s) const {
    return {x * s, y * s};
  }
};
inline double length(V v) {
  return std::hypot(v.x, v.y);
}
inline double ease(double t) {
  t = 1 - std::clamp(t, 0., 1.);
  return 1 - t * t * t;
}
struct Box {
  double l = 0, t = 0, r = 1, b = 1;
  V clip(V v) const {
    return {std::clamp(v.x, l, std::max(l, r)), std::clamp(v.y, t, std::max(t, b))};
  }
  bool has(V v) const {
    return v.x >= l && v.x <= r && v.y >= t && v.y <= b;
  }
};
struct World {
  Box screen, front;
  bool window = false, editor = false, browser = false, luogu = false, desktop = false, dark = false;
  V cursor;
};
bool ellipse(V local, double scale, bool sleep);
struct Walk {
  V pos{}, target{};
  double speed = 0, angle = 0, lean = 0, settle = 0;
  ms rest = 0;
  bool going = false, brake = false;
  void aim(V, Box);
  void stop();
  void warp(V, Box);
  void step(double dt, ms now, Box bounds, const World &, double factor, bool tame);
};
V chooseTarget(const World &, Box bounds, std::mt19937 &);
} // namespace pet

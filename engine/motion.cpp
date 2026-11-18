#include "motion.hpp"
namespace pet {
bool ellipse(V p, double s, bool sleep) {
  double x = p.x / ((sleep ? 20. : 30.) * s), y = p.y / ((sleep ? 30. : 45.) * s);
  return x * x + y * y <= 1.;
}
void Walk::aim(V v, Box b) {
  target = b.clip(v);
  going = true;
  brake = false;
}
void Walk::stop() {
  if (!brake) {
    settle = speed * .1;
    brake = true;
  }
  going = false;
}
void Walk::warp(V v, Box b) {
  pos = target = b.clip(v);
  going = brake = false;
  speed = lean = 0;
  rest = 0;
}
void Walk::step(double dt, ms now, Box b, const World &w, double factor, bool tame) {
  dt = std::clamp(dt, 0., .1);
  pos = b.clip(pos);
  if (!dt)
    return;
  target = b.clip(target);
  V diff = target - pos;
  double dist = length(diff);
  if (now < rest && !tame) {
    speed = 0;
    return;
  }
  double desired = (going && dist > .5) ? std::min(80. * factor, std::sqrt(600. * dist)) : 0.;
  double a = std::atan2(diff.y, diff.x);
  if (!tame && w.window && going && dist > 20) {
    V ahead = pos + V{std::cos(angle), std::sin(angle)} * (speed * .3);
    V edge = w.front.clip(ahead) - ahead;
    if (length(edge) < 20 && !w.front.has(pos) && length(edge) > .01) {
      double tangent = std::atan2(edge.y, edge.x) + pi * .5;
      if (std::cos(tangent - a) < 0)
        tangent += pi;
      a += std::clamp(std::remainder(tangent - a, 2 * pi), -pi / 6, pi / 6);
    }
  }
  double turn = std::clamp(std::remainder(a - angle, 2 * pi), -pi * .5 * dt, pi * .5 * dt);
  if (going) {
    angle += turn;
    lean += (turn / dt * 180 / pi * .15 - lean) * ease(dt / .4);
  } else
    lean += (0. - lean) * ease(dt / .4);
  speed += std::clamp(desired - speed, -(brake ? 400. : 300.) * dt, 200. * dt);
  double d = speed * dt;
  if (brake) {
    d = std::min(d, settle);
    settle -= d;
  }
  if (going && dist <= std::max(.75, d)) {
    pos = target;
    going = false;
    speed = 0;
  } else {
    V p = pos + V{std::cos(angle), std::sin(angle)} * d;
    pos = b.clip(p);
    if (length(pos - p) > .1) {
      speed = 0;
      if (!tame) {
        rest = now + 8000;
        going = false;
      } else
        angle = a;
    }
  }
}
V chooseTarget(const World &w, Box b, std::mt19937 &r) {
  auto real = [&](double a, double z) {
    return std::uniform_real_distribution<double>(a, std::max(a, z))(r);
  };
  int p = int(r() % 100);
  if (p < 30)
    return b.clip(
        {(r() % 2) ? w.screen.l + 50 : w.screen.r - 50, (r() % 2) ? w.screen.t + 50 : w.screen.b - 50});
  if (p < 50)
    return b.clip({real(b.l, b.r), w.screen.b - real(1, 20)});
  Box f = w.window ? w.front : w.screen;
  return b.clip({real(f.l - 30, f.r + 30), real(f.t - 30, f.b + 30)});
}
} // namespace pet

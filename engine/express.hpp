#pragma once
#include "rules.hpp"
#include <algorithm>
namespace pet {
enum class Tag : unsigned { neutral, celebrate, comfort, focus, curiosity, rest };
enum Feature : unsigned {
  success = 1,
  needsComfort = 2,
  working = 4,
  curious = 8,
  lowEnergy = 16,
  lateHour = 32
};
struct LabelDef {
  const char *name;
  int priority, coolMs, lengthMs;
  bool interruptible;
};
const LabelDef &label(Tag);
struct Intents {
  bool recognize = false, compose = false;
  Tag tag = Tag::neutral;
  ms since = 0;
  unsigned pending = 0;
  std::array<ms, 6> allow{};
  void cancel();
  bool event(unsigned mask, ms now);
  bool request(Tag, ms now);
  void tick(ms now);
};
struct Style {
  double intensity = .8, gaze = 1, tempo = 1;
};
std::optional<Style> policy(std::string_view);
double safeSlider(int control, double value, bool &clipped);
struct Drawpad {
  std::array<uint8_t, 512> bits{};
  void line(int x, int y, int a, int b, bool ink);
  bool black(int x, int y) const;
  std::string to64() const;
  static std::optional<Drawpad> from64(std::string_view);
};
struct BioClock {
  ms base = 0;
  int local = 0;
  void start(ms monotonic, int localSeconds);
  int seconds(ms monotonic) const;
  bool nap(ms monotonic) const;
};
struct Beat {
  double peak = 0, bpm = 0;
  bool muted = false;
  ms last = 0;
  void sample(ms now, double v, bool mute);
  void clear();

private:
  bool high = false;
  std::array<ms, 4> periods{};
  int n = 0;
};
// Three levels above a guaranteed non-parameter rhythm: rich / face / single channel / baseline.
struct Expression {
  double weight = 0, micro = 0, beat = 0;
  int tier = 0;
};
Expression envelope(Tag, ms elapsed, int channels, Style);
} // namespace pet

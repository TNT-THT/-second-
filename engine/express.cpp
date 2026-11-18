#include "express.hpp"
#include "event_lut.hpp"
#include "json.hpp"
#include <cmath>
namespace pet {
static const LabelDef defs[] = {
    {"NEUTRAL", 0, 300, 300, true},          {"CELEBRATE", 4, 30000, 1800, false},
    {"COMFORT", 5, 60000, 6000, false},      {"FOCUS_ACCOMPANY", 2, 10000, 4000, true},
    {"IDLE_CURIOSITY", 1, 8000, 2400, true}, {"REST_REMIND", 3, 30000, 4000, true}};
const LabelDef &label(Tag t) {
  return defs[unsigned(t)];
}
void Intents::cancel() {
  tag = Tag::neutral;
  pending = 0;
  since = 0;
  allow.fill(0);
}
bool Intents::event(unsigned mask, ms now) {
  if (mask >= 64 || (!recognize && !compose))
    return false;
  Tag t = recognize                         ? Tag(eventClass[mask])
          : (mask & needsComfort)           ? Tag::comfort
          : (mask & success)                ? Tag::celebrate
          : (mask & (lowEnergy | lateHour)) ? Tag::rest
          : (mask & working)                ? Tag::focus
          : (mask & curious)                ? Tag::curiosity
                                            : Tag::neutral;
  return request(t, now);
}
bool Intents::request(Tag t, ms now) {
  if (t == Tag::neutral || (!recognize && !compose) || unsigned(t) >= 6 || now < allow[unsigned(t)])
    return false;
  allow[unsigned(t)] = now + label(t).coolMs;
  if (tag != Tag::neutral && now < since + label(tag).lengthMs &&
      (!label(tag).interruptible || label(t).priority < label(tag).priority)) {
    pending |= 1u << unsigned(t);
    return false;
  }
  tag = t;
  since = now;
  return true;
}
void Intents::tick(ms now) {
  if (tag == Tag::neutral || now < since + label(tag).lengthMs)
    return;
  tag = Tag::neutral;
  for (unsigned i = 1; i < 6; ++i)
    if ((pending & (1u << i)) && defs[i].priority > label(tag).priority)
      tag = Tag(i);
  pending &= ~(1u << unsigned(tag));
  since = now;
}
double safeSlider(int n, double v, bool &clip) {
  static constexpr double lo[] = {0, 0, .7}, hi[] = {1, 1, 1.3};
  if (n < 0 || n > 2)
    n = 0;
  double out = std::isfinite(v) ? std::clamp(v, lo[n], hi[n]) : lo[n];
  clip = v != out;
  return out;
}
std::optional<Style> policy(std::string_view raw) {
  try {
    auto j = nlohmann::json::parse(raw);
    if (j.at("version") != 1)
      return {};
    auto sem = j.at("semantics");
    if (!sem.is_array() || sem.size() > 32)
      return {};
    for (auto &x : sem)
      if (!x.is_string() || x.get<std::string>().size() > 40)
        return {};
    bool c;
    return Style{safeSlider(0, j.at("intensity").get<double>(), c),
                 safeSlider(1, j.at("gaze").get<double>(), c), safeSlider(2, j.at("tempo").get<double>(), c)};
  } catch (...) {
    return {};
  }
}
Expression envelope(Tag t, ms delta, int channels, Style s) {
  if (t == Tag::neutral)
    return {};
  static constexpr float lut[] = {0, .24f, .49f, .70f, .84f, .93f, .98f, 1, 1,
                                  1, .98f, .94f, .85f, .68f, .46f, .22f, 0};
  static constexpr float pulse[] = {0,     .35f,  .75f,  1,     .86f,  .58f,  .22f,  0, -.22f,
                                    -.52f, -.75f, -.82f, -.72f, -.55f, -.31f, -.12f, 0};
  double idx = std::clamp(delta * 16. * s.tempo / label(t).lengthMs, 0., 16.);
  int k = std::min(15, int(idx));
  double f = idx - k;
  auto interp = [&](auto &a) { return (a[k] + (a[k + 1] - a[k]) * f) * s.intensity; };
  int level = channels >= 8 ? 3 : channels >= 3 ? 2 : channels >= 1 ? 1 : 0;
  return {interp(lut),
          interp(pulse) * (t == Tag::celebrate ? -8
                           : t == Tag::rest    ? 1
                                               : 3),
          interp(pulse), level};
}
void Drawpad::line(int x, int y, int a, int b, bool ink) {
  auto safe = [](int z) { return std::clamp(z, 0, 63); };
  x = safe(x);
  y = safe(y);
  a = safe(a);
  b = safe(b);
  int dx = std::abs(a - x), sx = x < a ? 1 : -1, dy = -std::abs(b - y), sy = y < b ? 1 : -1, err = dx + dy;
  while (true) {
    auto &v = bits[y * 8 + x / 8];
    uint8_t m = uint8_t(1u << (x % 8));
    if (ink)
      v |= m;
    else
      v &= ~m;
    if (x == a && y == b)
      break;
    int e = 2 * err;
    if (e >= dy) {
      err += dy;
      x += sx;
    }
    if (e <= dx) {
      err += dx;
      y += sy;
    }
  }
}
bool Drawpad::black(int x, int y) const {
  return x >= 0 && x < 64 && y >= 0 && y < 64 && (bits[y * 8 + x / 8] & (1u << (x % 8)));
}
static constexpr std::string_view abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string Drawpad::to64() const {
  std::string o;
  for (size_t i = 0; i < bits.size(); i += 3) {
    unsigned w = (bits[i] << 16) | (i + 1 < bits.size() ? bits[i + 1] << 8 : 0) |
                 (i + 2 < bits.size() ? bits[i + 2] : 0);
    o += abc[(w >> 18) & 63];
    o += abc[(w >> 12) & 63];
    o += i + 1 < bits.size() ? abc[(w >> 6) & 63] : '=';
    o += i + 2 < bits.size() ? abc[w & 63] : '=';
  }
  return o;
}
std::optional<Drawpad> Drawpad::from64(std::string_view v) {
  if (v.size() != 684 || v.back() != '=')
    return {};
  Drawpad d;
  size_t n = 0;
  for (size_t i = 0; i < v.size(); i += 4) {
    unsigned w = 0;
    for (int j = 0; j < 4; ++j) {
      auto k = abc.find(v[i + j]);
      if (k == abc.npos && !(i + j == v.size() - 1 && v[i + j] == '='))
        return {};
      w = (w << 6) | (k == abc.npos ? 0 : unsigned(k));
    }
    for (int sh : {16, 8, 0})
      if (n < d.bits.size())
        d.bits[n++] = uint8_t(w >> sh);
  }
  return d.to64() == v ? std::optional<Drawpad>(d) : std::nullopt;
}
void BioClock::start(ms rt, int t) {
  base = rt;
  local = t % 86400;
}
int BioClock::seconds(ms rt) const {
  return int((local + std::max<ms>(0, rt - base) / 1000) % 86400);
}
bool BioClock::nap(ms rt) const {
  int h = seconds(rt) / 3600;
  return h == 6;
}
void Beat::clear() {
  *this = Beat{};
}
void Beat::sample(ms t, double v, bool mute) {
  peak = std::clamp(v, 0., 1.);
  muted = mute;
  if (mute) {
    bpm = 0;
    n = 0;
    last = 0;
    high = false;
    return;
  }
  if (!high && peak >= .38) {
    ms period = t - last;
    if (!last || period >= 280) {
      if (last && period >= 300 && period <= 1200) {
        periods[n++ % 4] = period;
        if (n >= 4) {
          auto mm = std::minmax_element(periods.begin(), periods.end());
          double sum = 0;
          for (auto a : periods)
            sum += a;
          bpm = (*mm.second - *mm.first) <= sum * .0275 ? 240000 / sum : 0;
        }
      } else {
        n = 0;
        bpm = 0;
      }
      last = t;
    }
    high = true;
  }
  if (peak < .24)
    high = false;
  if (t - last > 1600)
    bpm = 0;
}
} // namespace pet

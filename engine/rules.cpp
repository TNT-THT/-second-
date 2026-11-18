#include "rules.hpp"
#include "json.hpp"
#include <algorithm>
#include <cstdio>
#include <ctime>
using json = nlohmann::json;
namespace pet {
static std::tm local(sec t) {
  std::time_t a = static_cast<time_t>(t);
  std::tm b{};
#ifdef _WIN32
  localtime_s(&b, &a);
#else
  localtime_r(&a, &b);
#endif
  return b;
}
Date civil(sec t) {
  auto a = local(t);
  return {a.tm_year + 1900, a.tm_mon + 1, a.tm_mday, a.tm_hour, a.tm_min, a.tm_wday};
}
std::string day(sec t) {
  auto a = local(t);
  char b[20]{};
  std::strftime(b, sizeof b, "%Y-%m-%d", &a);
  return b;
}
sec midnight(sec t, int days) {
  auto a = local(t);
  a.tm_hour = a.tm_min = a.tm_sec = 0;
  a.tm_mday += days;
  a.tm_isdst = -1;
  return std::mktime(&a);
}
sec sunday20(sec mon) {
  auto a = local(mon);
  --a.tm_mday;
  a.tm_hour = 20;
  a.tm_min = a.tm_sec = 0;
  a.tm_isdst = -1;
  return std::mktime(&a);
}
std::string iso(sec t) {
  std::time_t a = static_cast<time_t>(t);
  std::tm b{};
#ifdef _WIN32
  if (gmtime_s(&b, &a))
    return {};
#else
  if (!gmtime_r(&a, &b))
    return {};
#endif
  char out[32]{};
  std::strftime(out, sizeof out, "%Y-%m-%dT%H:%M:%SZ", &b);
  return out;
}
std::optional<sec> epoch(std::string_view v) {
  if (v.size() != 20)
    return {};
  std::tm t{};
  int a, b, c, h, m, s;
  if (std::sscanf(std::string(v).c_str(), "%4d-%2d-%2dT%2d:%2d:%2dZ", &a, &b, &c, &h, &m, &s) != 6 ||
      a < 1970 || a > 3000)
    return {};
  t.tm_year = a - 1900;
  t.tm_mon = b - 1;
  t.tm_mday = c;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
#ifdef _WIN32
  sec e = _mkgmtime(&t);
#else
  sec e = timegm(&t);
#endif
  if (e < 0 || iso(e) != v)
    return {};
  return e;
}
std::string fingerprint(std::string_view p, uint64_t bytes, uint64_t ft) {
  std::string k = "petdesk-metadata-v1";
  k += '\0';
  k += p;
  k += '\0';
  k += std::to_string(bytes);
  k += '\0';
  k += std::to_string(ft);
  return sha256(k).substr(0, 8);
}
std::optional<std::string> foodType(std::string_view p) {
  auto at = p.find_last_of('.');
  if (at == p.npos)
    return {};
  std::string e(p.substr(at + 1));
  for (auto &c : e)
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  if (e == "txt" || e == "cpp" || e == "json" || e == "png")
    return e;
  return {};
}
std::string mood(int n) {
  return n == 0 ? "sleeping" : n <= 20 ? "hungry" : "normal";
}
std::string encode(const State &s) {
  return json{{"satiety", s.satiety},
              {"state", mood(s.satiety)},
              {"last_active", iso(s.last)},
              {"born", iso(s.born)},
              {"active_ms", s.active},
              {"feed_cursor", s.cursor},
              {"rub_day", s.rubDay},
              {"rub_points", s.rubPoints},
              {"week_day", s.weekDay},
              {"strokes", s.strokes},
              {"guardian", s.guardian},
              {"streak", s.streak},
              {"return_after_shutdown", s.returnAfterShutdown}}
      .dump();
}
static bool dateField(const std::string &s) {
  return s.empty() || bool(epoch(s + "T00:00:00Z"));
}
std::optional<State> decodeState(std::string_view line) {
  try {
    auto j = json::parse(line);
    if (!j.is_object() || !j.at("satiety").is_number_integer())
      return {};
    State s;
    s.satiety = j.at("satiety");
    if (s.satiety < 0 || s.satiety > 100 || j.at("state") != mood(s.satiety))
      return {};
    auto last = epoch(j.at("last_active").get<std::string>());
    auto born = epoch(j.value("born", j.at("last_active").get<std::string>()));
    if (!last || !born)
      return {};
    s.last = *last;
    s.born = *born;
    s.active = j.value("active_ms", ms(0));
    s.rubDay = j.value("rub_day", std::string{});
    s.weekDay = j.value("week_day", std::string{});
    s.rubPoints = j.value("rub_points", 0);
    if (j.contains("feed_cursor") && !j["feed_cursor"].is_number_unsigned())
      return {};
    s.strokes = std::clamp(j.value("strokes", 0), 0, 1000000);
    s.guardian = j.value("guardian", false);
    s.streak = j.value("streak", false);
    s.returnAfterShutdown = j.value("return_after_shutdown", false);
    s.cursor = j.value("feed_cursor", uint64_t(0));
    if (!dateField(s.rubDay) || !dateField(s.weekDay) || s.active < 0 || s.active >= 1800000 ||
        s.rubPoints < 0 || s.rubPoints > 10)
      return {};
    return s;
  } catch (...) {
    return {};
  }
}
static bool hashOK(std::string_view h) {
  return h.size() == 8 && h.find_first_not_of("0123456789abcdef") == h.npos;
}
std::string encode(const Meal &f) {
  return json{{"ts", iso(f.ts)}, {"hash", f.hash}, {"type", f.type}}.dump();
}
std::optional<Meal> decodeMeal(std::string_view line) {
  try {
    auto j = json::parse(line);
    auto t = epoch(j.at("ts").get<std::string>());
    std::string h = j.at("hash"), e = j.at("type");
    if (!t || !hashOK(h) || foodType("x." + e) != e || j.size() != 3)
      return {};
    return Meal{*t, h, e};
  } catch (...) {
    return {};
  }
}
std::string encode(const Options &o) {
  return json{{"luogu", o.luogu},   {"input_counts", o.input}, {"birthday", o.birthday},
              {"intent", o.intent}, {"feedback", o.feedback},  {"audio_peak", o.meter},
              {"ime_mode", o.ime},  {"gamma", o.gamma},        {"weather", o.weather},
              {"voice", o.voice}}
      .dump();
}
std::optional<Options> decodeOptions(std::string_view line) {
  try {
    auto j = json::parse(line);
    if (!j.is_object())
      return {};
    Options o;
    auto allowed = json::parse(encode(o));
    for (auto &kv : j.items())
      if (!allowed.contains(kv.key()))
        return {};
    o.luogu = j.value("luogu", false);
    o.input = j.value("input_counts", false);
    o.birthday = j.value("birthday", std::string{});
    o.intent = j.value("intent", false);
    o.feedback = j.value("feedback", false);
    o.meter = j.value("audio_peak", false);
    o.ime = j.value("ime_mode", false);
    o.gamma = j.value("gamma", false);
    o.weather = j.value("weather", false);
    o.voice = j.value("voice", false);
    if (!o.birthday.empty() && !epoch("2000-" + o.birthday + "T00:00:00Z"))
      return {};
    return o;
  } catch (...) {
    return {};
  }
}
bool birthday(std::string_view v, Date d) {
  char b[8]{};
  std::snprintf(b, sizeof b, "%02d-%02d", d.m, d.d);
  return v == b;
}
// Gregorian dates of lunar month one, day one, 2020–2050. Only days 1–7 select the seasonal rig.
bool newYear(Date d) {
  static constexpr int dates[] = {125, 212, 201, 122, 210, 129, 217, 206, 126, 213, 203,
                                  123, 211, 131, 219, 208, 128, 215, 204, 124, 212, 201,
                                  122, 210, 130, 217, 206, 126, 214, 202, 123};
  if (d.y < 2020 || d.y > 2050)
    return false;
  int at = dates[d.y - 2020], m = at / 100, dd = at % 100;
  int distance = (d.m == m) ? d.d - dd : (m == 1 && d.m == 2 ? 31 - dd + d.d : 100);
  return distance >= 0 && distance < 7;
}
bool night(Date d) {
  return d.h >= 22 || d.h < 6;
}
bool late(Date d) {
  return d.h >= 23 || d.h < 6;
}
void Life::load(std::optional<State> v, sec now) {
  s = v.value_or(State{});
  if (!v) {
    s.satiety = 50;
    s.last = s.born = now;
    dream = true;
  }
  away = std::max<sec>(0, now - s.last);
}
void Life::apply(bool credited) {
  if (s.satiety == 0)
    s.satiety = 30;
  else if (credited)
    s.satiety = std::min(100, s.satiety + 20);
}
bool Life::seen(const Meal &m) const {
  return std::any_of(recent.begin(), recent.end(),
                     [&](const Meal &v) { return v.hash == m.hash && v.ts > m.ts - 86400; });
}
void Life::remember(const Meal &m, sec now) {
  if (m.ts >= now - 9 * 86400) {
    recent.push_back(m);
    if (recent.size() > 550)
      recent.pop_front();
  }
  lastMeal = std::max(lastMeal, m.ts);
  five.push_back(m.type);
  if (five.size() > 5)
    five.pop_front();
}
void Life::history(const Meal &m, uint64_t end, sec now, bool replay) {
  if (replay && end > s.cursor) {
    apply(!seen(m));
    s.cursor = end;
  }
  remember(m, now);
}
void Life::ready(sec now) {
  neglected = now - (lastMeal ? lastMeal : s.born) >= 3 * 86400;
  if (civil(now).wday == 1 && s.weekDay != day(now)) {
    sec a = midnight(now, -7), b = sunday20(now);
    weekly = std::count_if(recent.begin(), recent.end(),
                           [&](const Meal &m) { return m.ts >= a && m.ts <= b; }) >= 10;
    if (weekly)
      s.weekDay = day(now);
  }
  save(now);
}
int Life::today(sec t) const {
  sec a = midnight(t), b = midnight(t, 1);
  return int(
      std::count_if(recent.begin(), recent.end(), [&](const Meal &f) { return f.ts >= a && f.ts < b; }));
}
bool Life::save(sec now) {
  s.last = now;
  flush = 0;
  bool ok = out.snapshot(s);
  dream |= !ok;
  return ok;
}
FoodResult Life::feed(std::string h, std::string e, sec now) {
  if (!hashOK(h) || foodType("x." + e) != e)
    return FoodResult::no;
  if (today(now) >= 50)
    return FoodResult::full;
  recent.erase(std::remove_if(recent.begin(), recent.end(), [&](auto &m) { return m.ts < now - 9 * 86400; }),
               recent.end());
  Meal f{now, h, e};
  bool again = seen(f);
  auto end = out.meal(f);
  if (!end) {
    dream = true;
    return FoodResult::io;
  }
  apply(!again);
  s.cursor = *end;
  remember(f, now);
  bool seven = true;
  for (int i = 0; i < 7; ++i)
    seven &= today(midnight(now, -i)) > 0;
  s.streak |= seven;
  save(now);
  return again ? FoodResult::again : FoodResult::fresh;
}
void Life::tick(ms dt, bool active, bool chat, sec now) {
  if (!active || dt <= 0)
    return;
  flush += dt;
  if (!chat && s.satiety) {
    s.active += dt;
    if (s.active >= 1800000) {
      auto loss = (s.active / 1800000) * 5;
      s.satiety = int(std::max<ms>(0, s.satiety - loss));
      s.active %= 1800000;
      save(now);
    }
  }
  if (flush >= 5000)
    save(now);
}
bool Life::wake(sec t) {
  if (s.satiety)
    return false;
  s.satiety = 30;
  save(t);
  return true;
}
bool Life::stroke(sec now) {
  auto d = day(now);
  if (d != s.rubDay) {
    s.rubDay = d;
    s.rubPoints = 0;
  }
  s.strokes = std::min(1000000, s.strokes + 1);
  if (s.rubPoints >= 10) {
    save(now);
    return false;
  }
  s.rubPoints += 5;
  s.satiety = std::min(100, s.satiety + 5);
  save(now);
  return true;
}
std::vector<std::string> Life::lastFive() const {
  return {five.rbegin(), five.rend()};
}
void Rates::reset() {
  *this = Rates{};
}
void Rates::count(bool key) {
  auto &v = key ? k : c;
  if (v < 255)
    ++v;
}
bool Rates::advance(int64_t s) {
  if (prev < 0) {
    prev = s;
    return false;
  }
  if (s == prev)
    return false;
  if (s - prev != 1) {
    reset();
    prev = s;
    return false;
  }
  prev = s;
  keys[i] = k;
  clicks[i] = c;
  i = (i + 1) % 180;
  n = std::min(180, n + 1);
  k = c = 0;
  bool fast = n >= 5;
  for (int j = 0; j < 5 && fast; ++j)
    fast &= clicks[(i + 179 - j) % 180] >= 8;
  bool slow = n == 180;
  for (int block = 0; block < 3 && slow; ++block) {
    int sum = 0;
    for (int j = 0; j < 60; ++j)
      sum += keys[(i + block * 60 + j) % 180];
    slow = sum < 10;
  }
  return fast || slow;
}
} // namespace pet

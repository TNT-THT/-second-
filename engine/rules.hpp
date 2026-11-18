#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace pet {
using ms = int64_t;
using sec = int64_t;
struct Date {
  int y, m, d, h, min, wday;
}; // Sunday = 0, local civil time
Date civil(sec);
std::string day(sec);
std::string iso(sec);
std::optional<sec> epoch(std::string_view);
sec midnight(sec, int days = 0);
sec sunday20(sec monday);
std::string sha256(std::string_view);
std::string fingerprint(std::string_view normalized, uint64_t bytes, uint64_t modified);
std::optional<std::string> foodType(std::string_view filename);
struct DeniedOperations final {
  static constexpr bool erase() {
    return false;
  }
  static constexpr bool move() {
    return false;
  }
  static constexpr bool replace() {
    return false;
  }
  static constexpr bool encrypt() {
    return false;
  }
};
struct State {
  int satiety = 100;
  sec last = 0, born = 0;
  ms active = 0;
  uint64_t cursor = 0;
  std::string rubDay, weekDay;
  int rubPoints = 0, strokes = 0;
  bool guardian = false, streak = false, returnAfterShutdown = false;
};
std::string mood(int);
std::string encode(const State &);
std::optional<State> decodeState(std::string_view);
struct Meal {
  sec ts;
  std::string hash, type;
};
std::string encode(const Meal &);
std::optional<Meal> decodeMeal(std::string_view);
struct Options {
  bool luogu = false, input = false, intent = false, feedback = false, meter = false, ime = false,
       gamma = false, weather = false, voice = false;
  std::string birthday;
};
std::string encode(const Options &);
std::optional<Options> decodeOptions(std::string_view);
enum class FoodResult { fresh, again, full, no, io };
// Platform implements only these two append capabilities. Offsets are durable feed-log byte ends.
struct Journal {
  virtual std::optional<uint64_t> meal(const Meal &) = 0;
  virtual bool snapshot(const State &) = 0;
  virtual ~Journal() = default;
};
struct Life {
  State s;
  Journal &out;
  bool dream = false;
  sec lastMeal = 0, away = 0;
  bool neglected = false, weekly = false;
  explicit Life(Journal &j) : out(j) {};
  void load(std::optional<State>, sec now);
  void history(const Meal &, uint64_t end, sec now, bool replay);
  void ready(sec now);
  bool save(sec);
  FoodResult feed(std::string hash, std::string type, sec now);
  void tick(ms delta, bool active, bool chat, sec now);
  bool wake(sec);
  bool stroke(sec);
  int today(sec) const;
  std::vector<std::string> lastFive() const;

private:
  std::deque<Meal> recent;
  std::deque<std::string> five;
  ms flush = 0;
  bool seen(const Meal &) const;
  void remember(const Meal &, sec now);
  void apply(bool credited);
};
bool birthday(std::string_view, Date);
bool newYear(Date);
bool night(Date);
bool late(Date);
struct Rates {
  void reset();
  void count(bool key);
  bool advance(int64_t second);

private:
  std::array<uint8_t, 180> keys{}, clicks{};
  uint8_t k = 0, c = 0;
  int i = 0, n = 0;
  int64_t prev = -1;
};
} // namespace pet

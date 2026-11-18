#pragma once
#include "motion.hpp"
#include "express.hpp"
#include <map>
namespace pet {
enum class Mode { free, work, chat };
enum class Clip {
  stand,
  eat,
  shake,
  jump,
  vsign,
  rub,
  dodge,
  limp,
  stagger,
  sway,
  sit,
  back,
  belly,
  calm,
  congrats,
  nod,
  stretch,
  spin,
  reunion,
  miss
};
enum class Sound { chime, satisfied, sigh, sob, steps, birthday, cracker, weekly, ime };
enum class Fx { none, star, flowers, heart, lantern };
struct Signal {
  Sound sound;
  double db;
};
struct Effect {
  Fx kind = Fx::none;
  ms at = 0, len = 0;
};
struct Pose {
  double breath = 0, eye = 1, gx = 0, gy = 0, jaw = 0, smile = 0, surprise = 0, cup = 0, blush = 0, lean = 0,
         jump = 0, offset = 0, wave = 0, vsign = 0, back = 0, spin = 0, sit = 0, hand = 0, limp = 0, rub = 0,
         skirt = 0, step = 0, light = 1, saturation = 1, highlights = 1, hair = 0, nod = 0, wind = 0;
  bool spring = false, halloween = false, rain = false;
  double hue = 0, highlightSat = 1, eyesSmile = 0, bodyYaw = 0;
  Tag tag = Tag::neutral;
  Expression expression;
  int track = 0;
  ms trackTime = 0;
  double gazeGain = 1;
};
struct Scene {
  Life &life;
  Options opt;
  Intents intents;
  Style style;
  Beat beat;
  int capability = 0;
  bool trusted = false, lowBattery = false, shallow = false, imeChinese = false, imeKnown = false,
       rain = false, sun = false;
  double warm = 0, cold = 0;
  ms dreamGuard = 0;
  void smartStop();
  void observeDoodle(int);
  void stretch();
  void farewell();
  void memo(bool ac, bool wa);
  std::mt19937 rng;
  Walk walk;
  World w;
  Box bounds;
  ms now = 0, start = 0, end = 0;
  sec wall = 0;
  Mode mode = Mode::free;
  Clip clip = Clip::stand;
  Effect effect;
  std::vector<Signal> audio;
  double scale = 1, goalScale = 1;
  ms scaleLabel = 0;
  bool mute = false, drag = false, lateWork = false;
  int replies = 0;
  std::vector<std::wstring> lines;
  // Wording comes from config/feed_limits.json; the scene never invents a limit on its own.
  std::wstring heavyLine = L"这个太重了吃不下…";
  explicit Scene(Life &, uint32_t seed);
  void init(World, sec);
  void tick(ms dt, sec, World);
  void setMode(Mode);
  void reset();
  void food(FoodResult, bool real = true);
  void press(V);
  void move(V);
  void release();
  void dragBegin();
  void dragEnd();
  void wheel(int);
  void cancel();
  void respond();
  void calm();
  void accepted(const std::string &);
  void talk(std::wstring, ms hold = 3000, ms delay = 0, int charMs = 0);
  std::wstring caption() const;
  void sound(Sound, double);
  Pose pose() const;
  bool busy() const {
    return now < end;
  }
  bool activeVisual() const {
    return busy() || walk.going || walk.speed > .1 || left || singleAt >= 0 ||
           std::abs(scale - goalScale) > .001 || now < effect.at + effect.len;
  }

private:
  std::wstring words;
  ms wordsAt = 0, wordsEnd = 0;
  int typing = 0;
  std::array<ms, 3> cool{};
  ms idleAt = 0, roamAt = 0, bellyAt = 60000, chatAt = 0, nodAt = 0, nightAt = 0, starAt = 0, calmAt = 0,
     stageAt = 0, stepAt = 0;
  std::map<std::string, ms> wins;
  std::deque<int> stages;
  bool left = false, petted = false, dodged = false, desktop = false, extra = false;
  ms deskAt = 0, leftAt = 0, movedAt = 0, singleAt = -1, lastUp = -1000, scaleAt = 0;
  double oldScale = 1, blush = 0, gx = 0, gy = 0, hair = 0, hairV = 0;
  V finger{}, origin{}, singlePos{}, rubOffset{};
  int blinkCycle = 4000;
  ms blinkAt = 3800;
  std::string celebrationDay;
  bool walked = false;
  V resume{};
  int range(int, int);
  void action(Clip, ms);
  void stage();
  void selectBounds();
  void hit();
  void fx(Fx, ms);
  void interrupt();
  V bottom() const;
  bool activePet() const {
    return mode != Mode::work && !drag;
  }
};
} // namespace pet

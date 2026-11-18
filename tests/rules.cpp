#include "rules.hpp"
#include "feed.hpp"
#include "layout.hpp"
#include "models.hpp"
#include "scene.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
using namespace pet;
#define CHECK(x)                                                                                             \
  do {                                                                                                       \
    if (!(x))                                                                                                \
      throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " #x);                   \
  } while (0)
struct Memory : Journal {
  std::vector<Meal> meals;
  std::vector<State> states;
  bool fail = false;
  std::optional<uint64_t> meal(const Meal &m) override {
    if (fail)
      return {};
    meals.push_back(m);
    return meals.size() * 60;
  }
  bool snapshot(const State &s) override {
    if (fail)
      return false;
    states.push_back(s);
    return true;
  }
};
static sec base() {
  return *epoch("2026-11-18T12:00:00Z");
}
struct Rig {
  Memory j;
  Life l{j};
  Scene s{l, 42};
  World w{{0, 0, 1920, 1040}, {300, 200, 1200, 760}, true, true, false, false, false, false, {710, 460}};
  explicit Rig(int n = 75) {
    State a;
    a.satiety = n;
    a.born = a.last = base();
    l.load(a, base());
    l.ready(base());
    s.init(w, base());
  }
  void step(int n) {
    while (n > 0) {
      int d = std::min(n, 25);
      s.tick(d, base() + (s.now + d) / 1000, w);
      n -= d;
    }
  }
};
int main() {
#ifdef _WIN32
  _putenv_s("TZ", "UTC0");
  _tzset();
#else
  setenv("TZ", "UTC", 1);
  tzset();
#endif
  int tests = 0;
  auto test = [&](const char *n, const std::function<void()> &fn) {
    try {
      fn();
      ++tests;
      std::printf("ok %02d - %s\n", tests, n);
    } catch (const std::exception &e) {
      std::fprintf(stderr, "FAIL %s: %s\n", n, e.what());
      std::exit(1);
    }
  };
  test("file mutations are inert by construction", [] {
    static_assert(!DeniedOperations::erase() && !DeniedOperations::move() && !DeniedOperations::encrypt() &&
                  !DeniedOperations::replace());
  });
  test("hash vectors and metadata domain separation", [] {
    CHECK(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256(std::string(1000, 'a')) ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    CHECK(fingerprint("a", 1, 20) != fingerprint("a", 12, 0));
    CHECK(fingerprint("a", 1, 20).size() == 8);
  });
  test("suffix whitelist, case and double extensions", [] {
    for (auto s : {"note.TXT", ".a.cpp", "image.pNg", "note.json"})
      CHECK(foodType(s));
    for (auto s : {"a.gif", "x.txt.exe", "txt", "p.png/secret", "read.me.txt "})
      CHECK(!foodType(s));
  });
  test("bounded UTC ISO records and corrupt state", [] {
    State s;
    s.born = s.last = base();
    CHECK(decodeState(encode(s))->satiety == 100);
    CHECK(!decodeState("{}"));
    CHECK(!decodeState("{\"satiety\":16,\"state\":\"normal\",\"last_active\":\"2026-10-10T10:00:00Z\"}"));
    CHECK(!epoch("2026-02-30T00:00:00Z"));
    CHECK(iso(*epoch("2024-02-29T00:00:00Z")) == "2024-02-29T00:00:00Z");
  });
  test("opt-ins separate, private month/day only", [] {
    auto d = decodeOptions("{}");
    CHECK(d && !d->input && !d->luogu && d->birthday.empty());
    auto o = decodeOptions("{\"luogu\":true,\"birthday\":\"02-29\"}");
    CHECK(o && o->luogu && !o->input);
    CHECK(!decodeOptions("{\"birthday\":\"2000-10-01\"}"));
    CHECK(!decodeOptions("{\"input_counts\":1}"));
    CHECK(!decodeOptions("{\"endpoint\":\"any\"}"));
  });
  test("missing state exception, no simulated offline decay", [] {
    Memory j;
    Life l(j);
    l.load({}, base());
    CHECK(l.s.satiety == 50 && l.dream && l.s.last == base());
    auto s = l.s;
    s.satiety = 100;
    l.load(s, base() + 86400 * 10);
    CHECK(l.s.satiety == 100 && l.away == 10 * 86400);
  });
  test("real-only writes, log projection and rolling boundary", [] {
    Rig a(10);
    auto &l = a.l;
    CHECK(l.feed("1234abcd", "gif", base()) == FoodResult::no);
    CHECK(a.j.meals.empty());
    CHECK(l.feed("1234abcd", "cpp", base()) == FoodResult::fresh && l.s.satiety == 30);
    CHECK(l.feed("1234abcd", "cpp", base() + 2) == FoodResult::again && l.s.satiety == 30);
    CHECK(l.feed("1234abcd", "cpp", base() + 86401) == FoodResult::again);
    CHECK(l.feed("1234abcd", "cpp", base() + 86401 + 86400) == FoodResult::fresh && l.s.satiety == 50);
    CHECK(encode(a.j.meals[0]) == "{\"hash\":\"1234abcd\",\"ts\":\"2026-11-18T12:00:00Z\",\"type\":\"cpp\"}");
  });
  test("50 accepted/day, midnight reset, last 5 only", [] {
    Rig a;
    for (int i = 0; i < 50; ++i)
      CHECK(a.l.feed("0000abcd", i % 2 ? "cpp" : "txt", base() + i) != FoodResult::full);
    CHECK(a.l.today(base()) == 50);
    CHECK(a.l.feed("0000abcd", "cpp", base() + 99) == FoodResult::full);
    CHECK(a.j.meals.size() == 50);
    auto types = a.l.lastFive();
    CHECK(types.size() == 5 && types[0] == "cpp" && types[1] == "txt");
    CHECK(a.l.feed("0000abcd", "cpp", midnight(base(), 1)) == FoodResult::again);
    CHECK(a.l.today(midnight(base(), 1)) == 1);
  });
  test("log failure fails closed without giving a meal", [] {
    Rig a(50);
    a.j.fail = true;
    CHECK(a.l.feed("12345678", "png", base()) == FoodResult::io && a.l.s.satiety == 50 && a.l.dream);
  });
  test("durable cursor replays exactly the gap", [] {
    Rig a(25);
    Meal m{base(), "aa808080", "png"};
    a.l.s.cursor = 100;
    a.l.history(m, 100, base(), true);
    CHECK(a.l.s.satiety == 25);
    a.l.history({base() + 1, m.hash, m.type}, 160, base(), true);
    CHECK(a.l.s.satiety == 25 && a.l.s.cursor == 160);
    a.l.history({base() + 2, "00ababab", "cpp"}, 220, base(), true);
    CHECK(a.l.s.satiety == 45 && a.l.s.cursor == 220);
  });
  test("five-second heartbeat; partial half-hours survive restart", [] {
    Rig a(26);
    auto n = a.j.states.size();
    a.l.tick(4999, true, false, base());
    CHECK(a.j.states.size() == n);
    a.l.tick(1, true, false, base());
    CHECK(a.j.states.size() == n + 1);
    a.l.tick(1800000 - 5000, true, false, base());
    CHECK(a.l.s.satiety == 21 && a.l.s.active == 0);
    a.l.tick(1800000, false, false, base());
    a.l.tick(1800000, true, true, base());
    CHECK(a.l.s.satiety == 21 && a.l.s.active == 0);
    a.l.tick(1800000, true, false, base());
    CHECK(mood(a.l.s.satiety) == "hungry");
  });
  test("sleeping eats or clicks straight to thirty", [] {
    Rig a(0);
    CHECK(a.l.feed("01010101", "json", base()) == FoodResult::fresh && a.l.s.satiety == 30);
    a.l.s.satiety = 0;
    a.s.press({});
    CHECK(a.l.s.satiety == 30);
    CHECK(mood(20) == "hungry" && mood(21) == "normal" && mood(0) == "sleeping");
  });
  test("stroking credits capped per local day", [] {
    Rig a(50);
    CHECK(a.l.stroke(base()) && a.l.stroke(base()));
    CHECK(!a.l.stroke(base()) && a.l.s.satiety == 60);
    CHECK(a.l.stroke(midnight(base(), 1)) && a.l.s.satiety == 65);
  });
  test("weekly Sunday 20 cutoff / Monday claim once", [] {
    auto mon = *epoch("2026-11-23T09:00:00Z");
    Rig a;
    for (int i = 0; i < 9; ++i)
      a.l.history({mon - 3 * 86400 + i, "12345678", "txt"}, 60 * i, mon, false);
    a.l.history({sunday20(mon) + 1, "12345678", "txt"}, 900, mon, false);
    a.l.ready(mon);
    CHECK(!a.l.weekly);
    a.l.history({sunday20(mon), "aabbccdd", "png"}, 960, mon, false);
    a.l.ready(mon);
    CHECK(a.l.weekly && a.l.s.weekDay == day(mon));
    Life next(a.j);
    next.load(a.l.s, mon);
    next.ready(mon);
    CHECK(!next.weekly);
  });
  test("offline story is per session, not a disk flag", [] {
    auto t = base() + 8 * 86400;
    Rig a;
    Life l(a.j);
    l.load(a.l.s, t);
    l.ready(t);
    CHECK(l.neglected && l.away == 8 * 86400);
    Scene s(l, 1);
    s.init(a.w, t);
    CHECK(s.clip == Clip::reunion && s.audio.size() == 1 && s.audio[0].sound == Sound::sob);
    CHECK(encode(l.s).find("neglected") == std::string::npos);
  });
  test("ellipse boundary and sleep collider", [] {
    CHECK(ellipse({30, 0}, 1, false) && !ellipse({30, 1}, 1, false));
    CHECK(ellipse({0, 30}, 1, true) && !ellipse({21, 0}, 1, true));
    CHECK(ellipse({45, 0}, 1.5, false));
  });
  test("ingestion phases and immediate return, virtual no writes", [] {
    Rig a;
    auto n = a.j.meals.size();
    a.s.food(FoodResult::again, false);
    CHECK(a.s.pose().cup == 1 && a.s.pose().eye == 1.2);
    a.step(550);
    CHECK(std::abs(a.s.pose().jaw - 8) < .001);
    a.step(2750);
    CHECK(a.s.audio.back().sound == Sound::satisfied);
    a.step(200);
    CHECK(a.s.clip == Clip::stand && a.j.meals.size() == n);
  });
  test("single, double, ten-second star cooldown", [] {
    Rig a(85);
    a.s.press({});
    a.s.release();
    a.step(310);
    CHECK(a.s.clip == Clip::jump);
    a.step(600);
    auto twice = [&] {
      a.s.press({});
      a.s.release();
      a.step(200);
      a.s.press({});
      a.s.release();
    };
    twice();
    CHECK(a.s.clip == Clip::vsign && a.s.effect.kind == Fx::star);
    auto at = a.s.effect.at;
    a.step(2000);
    twice();
    CHECK(a.s.effect.at == at);
    a.step(10000);
    twice();
    CHECK(a.s.effect.at > at);
  });
  test("hold, touch speed and drag exclusivity", [] {
    Rig a(50);
    a.s.press({});
    a.step(825);
    CHECK(a.s.clip == Clip::rub && a.s.pose().eye == 0);
    a.s.release();
    CHECK(a.l.s.satiety == 55);
    a.s.press({});
    a.step(850);
    a.s.move({220, 0});
    CHECK(a.s.clip == Clip::dodge);
    a.s.release();
    CHECK(a.l.s.satiety == 55);
    a.s.dragBegin();
    a.s.press({});
    a.s.release();
    a.step(1000);
    CHECK(a.s.clip == Clip::limp);
    a.s.dragEnd();
    a.step(501);
    CHECK(a.s.clip == Clip::stand);
  });
  test("wheel easing/limits and reset anchored at bottom", [] {
    Rig a;
    auto p = a.s.walk.pos;
    a.s.wheel(9);
    a.step(300);
    CHECK(a.s.scale == 1.5);
    a.s.wheel(-20);
    a.step(300);
    CHECK(a.s.scale == .8);
    a.s.reset();
    CHECK(a.s.scale == 1 && length(p - a.s.walk.pos) < .1);
  });
  test("work disables idle; chat pauses decay but allows feeding", [] {
    Rig a;
    a.s.setMode(Mode::work);
    a.step(60050);
    CHECK(a.s.mode == Mode::work && a.s.clip == Clip::nod);
    a.s.setMode(Mode::chat);
    auto acc = a.l.s.active;
    a.s.lines = {L"本地的一行"};
    a.step(112000);
    CHECK(a.l.s.active == acc);
    a.s.talk(L"你好", 6000);
    for (int i = 0; i < 8; ++i)
      a.s.respond();
    CHECK(a.s.replies == 5);
    CHECK(a.l.feed("aabbccdd", "png", base()) == FoodResult::fresh);
  });
  test("click-rate evidence requires five full seconds", [] {
    Rates r;
    r.advance(0);
    for (int i = 1; i <= 5; ++i) {
      for (int n = 0; n < 8; ++n)
        r.count(false);
      CHECK(r.advance(i) == (i == 5));
    }
    r.reset();
    CHECK(!r.advance(999));
  });
  test("quiet trigger requires three complete low-key minutes", [] {
    Rates r;
    r.advance(0);
    for (int i = 1; i <= 180; i++) {
      if (i == 60)
        for (int n = 0; n < 10; n++)
          r.count(true);
      CHECK(!r.advance(i));
    }
    for (int i = 181; i <= 240; i++) {
      bool b = r.advance(i);
      if (i == 240)
        CHECK(b);
    }
    r.advance(1000);
    CHECK(!r.advance(1001));
  });
  test("context features independent/default denied and cooldowns", [] {
    Rig a;
    a.w.browser = a.w.luogu = true;
    a.step(1);
    a.s.accepted("P1000");
    a.s.calm();
    CHECK(a.s.clip == Clip::stand);
    a.s.opt.luogu = true;
    a.s.accepted("bad/id");
    CHECK(a.s.clip == Clip::stand);
    a.s.accepted("P1000");
    CHECK(a.s.clip == Clip::congrats);
    a.step(2001);
    a.s.accepted("P1000");
    CHECK(a.s.clip == Clip::stand);
    a.s.opt.input = true;
    a.s.calm();
    CHECK(a.s.clip == Clip::calm);
    a.step(6001);
    a.s.calm();
    CHECK(a.s.clip == Clip::stand);
  });
  test("motion: finite turns, caps, bounded stop and screen margins", [] {
    World w;
    Box b{10, 10, 990, 990};
    Walk a;
    a.warp({500, 500}, b);
    a.aim({0, 50}, b);
    for (int i = 0; i < 150; ++i) {
      double old = a.angle, sp = a.speed;
      a.step(.025, i * 25, b, w, 1, false);
      CHECK(std::abs(a.angle - old) <= pi * .5 * .025 + .00001 && a.speed <= 80 && a.speed - sp <= 5.001 &&
            b.has(a.pos));
    }
    auto at = a.pos;
    a.stop();
    for (int i = 0; i < 20; i++)
      a.step(.025, 5000 + i * 25, b, w, 1, false);
    CHECK(a.speed == 0 && length(at - a.pos) <= 15);
  });
  test("calendar gates and seasonal rollback", [] {
    CHECK(newYear({2026, 2, 17, 0, 0, 0}) && newYear({2026, 2, 23, 0, 0, 0}) &&
          !newYear({2026, 2, 24, 0, 0, 0}));
    CHECK(newYear({2025, 2, 1, 0, 0, 0}));
    CHECK(birthday("10-31", {2026, 10, 31, 12, 0, 6}));
    CHECK(night({2026, 5, 1, 22, 0, 5}) && !late({2026, 5, 1, 22, 0, 5}) && late({2026, 5, 1, 5, 59, 5}));
  });
  test("independent intent/feedback toggles plus all six-by-six transitions", [] {
    for (int a = 0; a < 6; a++)
      for (int b = 0; b < 6; b++) {
        Intents h;
        CHECK(!h.event(success, 0));
        h.recognize = true;
        h.request(Tag(a), 0);
        h.request(Tag(b), 10);
        h.tick(10000);
        CHECK(unsigned(h.tag) < 6);
        h.cancel();
        CHECK(h.tag == Tag::neutral && h.pending == 0);
      }
    Intents h;
    h.compose = true;
    CHECK(h.event(working, 150) && h.tag == Tag::focus);
  });
  test("priority queue is bounded, cooldown doesn't restart animation", [] {
    Intents h;
    h.recognize = true;
    CHECK(h.event(success, 0));
    for (int i = 1; i < 1800; i++)
      CHECK(!h.event(success, i));
    CHECK(!h.event(needsComfort, 100) && h.tag == Tag::celebrate);
    h.tick(1801);
    CHECK(h.tag == Tag::comfort);
    for (int i = 0; i < 100000; ++i)
      h.request(Tag(1 + i % 5), 1801);
    CHECK(h.pending < 64);
    h.tick(7802);
    CHECK(h.tag != Tag::comfort);
    CHECK(!h.request(Tag::comfort, 60099));
    CHECK(h.request(Tag::comfort, 60100));
  });
  test("four capacity tiers retain an independent micro rhythm", [] {
    for (int t = 1; t < 6; ++t)
      for (auto n : {0, 1, 3, 10}) {
        auto v = envelope(Tag(t), 400, n, Style{});
        CHECK(v.tier == (n == 0 ? 0 : n == 1 ? 1 : n == 3 ? 2 : 3));
        CHECK(v.weight > 0 && v.weight <= 1 && std::isfinite(v.micro) && v.micro != 0);
      }
  });
  test("bad slider values and semantic-only policy inputs", [] {
    bool clipped = false;
    CHECK(safeSlider(2, 99, clipped) == 1.3 && clipped);
    CHECK(safeSlider(0, -10, clipped) == 0 && clipped);
    CHECK(safeSlider(1, NAN, clipped) == 0 && clipped);
    CHECK(!policy("{bad"));
    auto s = policy("{\"version\":1,\"intensity\":9,\"gaze\":-1,\"tempo\":0,\"semantics\":[\"breath\"]}");
    CHECK(s && s->intensity == 1 && s->gaze == 0 && s->tempo == .7);
  });
  test("binary art exactly 512 bytes / 684 base64 chars; invalid padding refused", [] {
    Drawpad d;
    d.line(-50, -20, 90, 82, true);
    for (int i = 0; i < 64; ++i)
      CHECK(d.black(i, i));
    d.line(2, 2, 4, 4, false);
    CHECK(!d.black(3, 3));
    auto b = d.to64();
    CHECK(b.size() == 684 && Drawpad::from64(b)->bits == d.bits);
    CHECK(!Drawpad::from64(b + "=") && !Drawpad::from64(b.substr(1)));
  });
  test("anchored circadian cycle ignores wall-clock corrections", [] {
    BioClock c;
    c.start(789000, 5 * 3600 + 59 * 60 + 59);
    CHECK(!c.nap(789000) && c.nap(790000));
    CHECK(!c.nap(790000 + 3600000) && c.nap(790000 + 86400000));
  });
  test("BPM from stable peak intervals only, quiet resets", [] {
    Beat b;
    for (int i = 1; i <= 8; ++i) {
      b.sample(i * 500, .84, false);
      b.sample(i * 500 + 80, .1, false);
    }
    CHECK(std::abs(b.bpm - 120) < .01);
    b.sample(4200, .98, true);
    CHECK(b.muted && b.bpm == 0);
  });
  test("doodle watching held three seconds, reboot marker consumed once", [] {
    Rig a;
    a.s.observeDoodle(0);
    CHECK(a.s.clip == Clip::sit && a.s.caption().empty());
    a.step(3000);
    CHECK(a.s.caption() == L"好可爱！");
    a.l.s.returnAfterShutdown = true;
    Scene next(a.l, 12);
    next.init(a.w, base());
    CHECK(next.caption() == L"等你好久啦" && !a.l.s.returnAfterShutdown);
  });

  test("feed limits default to 50 MiB and reject bad configuration", [] {
    auto d = parseFeedLimits("{}");
    CHECK(d && d->maxBytes == 52428800ull && d->recycle);
    CHECK(feedVerdict(52428800ull, *d) == FeedVerdict::ok);
    CHECK(feedVerdict(52428801ull, *d) == FeedVerdict::tooBig);
    auto c = parseFeedLimits("{\"max_bytes\":1024,\"recycle\":false,\"failed_line\":\"搬不动\",\"heavy_line\":\"太沉\"}");
    CHECK(c && c->maxBytes == 1024 && !c->recycle && c->failedLine == "搬不动" && c->heavyLine == "太沉");
    CHECK(!parseFeedLimits("[]"));
    CHECK(!parseFeedLimits("{\"max_bytes\":1023}"));
    CHECK(!parseFeedLimits("{\"max_bytes\":8589934593}"));
    CHECK(!parseFeedLimits("{\"max_bytes\":\"50\"}"));
    CHECK(!parseFeedLimits("{\"recycle\":1}"));
    CHECK(!parseFeedLimits("{\"heavy_line\":\"a\r\nb\"}"));
    CHECK(!parseFeedLimits("{\"failed_line\":\"\"}"));
    CHECK(!parseFeedLimits("{\"failed_line\":\"\r\n\"}"));
    CHECK(trashStamp("2026-11-18 11:51:09", "cake.png") == "2026-11-18T11-51-09_cake.png");
    CHECK(trashStamp("", "cake.png") == "cake.png");
  });
  test("bubbles stay fully inside the work area and keep the tip on the box", [] {
    RectI work{0, 0, 1920, 1040};
    auto a = placeBubble(work, 400, 600, 220, 64);
    CHECK(!a.below);
    CHECK(a.box.l >= 6 && a.box.r <= 1914 && a.box.t >= 6 && a.box.b <= 1034);
    CHECK(a.box.b <= 600 - 8 + 1 && a.tipX >= a.box.l + 18 && a.tipX <= a.box.r - 18);
    auto top = placeBubble(work, 400, 20, 220, 64);
    CHECK(top.below && top.box.t >= 28);
    auto edge = placeBubble(work, 1900, 600, 240, 64);
    CHECK(edge.box.r <= 1914 && edge.tipX <= edge.box.r - 18);
    auto left = placeBubble(work, -120, 300, 220, 64);
    CHECK(left.box.l >= 6 && left.tipX >= left.box.l + 18);
    RectI negative{-1920, -200, 0, 880};
    auto n = placeBubble(negative, -100, 300, 220, 64);
    CHECK(n.box.l >= -1914 && n.box.r <= -6 && n.box.b <= 880 - 6 && n.tipX >= n.box.l + 18);
    auto tiny = placeBubble(RectI{0, 0, 100, 40}, 50, 20, 300, 90);
    CHECK(tiny.box.r <= 94 && tiny.box.b <= 34 && tiny.box.width() == 88 && tiny.box.height() == 28);
    auto wide = placeBubble(RectI{0, 0, 120, 900}, 60, 880, 200, 60);
    CHECK(wide.below == false && wide.box.b <= 894);
  });
  test("model inspection names missing files and only binds real channels", [] {
    auto files = std::map<std::string, std::string>{
        {"Haru.model3.json", R"({"FileReferences":{"Moc":"Haru.moc3",
          "Textures":["Haru.2048/texture_00.png"],
          "Physics":"Haru.physics3.json",
          "Expressions":[{"Name":"F01","File":"exp/F01.exp3.json"}]}})"},
        {"Haru.cdi3.json", R"({"Parameters":[{"Id":"ParamAngleX"},{"Id":"ParamAngleY"},
          {"Id":"ParamAngleZ"},{"Id":"ParamBreath"},{"Id":"ParamBodyAngleZ"},
          {"Id":"ParamHairFront"},{"Id":"ParamHandDhangeL"}]})"},
        {"Haru.physics3.json", R"({"PhysicsSettings":[{"Output":[
          {"Destination":{"Id":"ParamHairFront"}},
          {"Destination":{"Id":"ParamAngleZ"}}]}]})"},
        {"Haru.moc3", "moc"},
        {"Haru.2048/texture_00.png", "png"},
        {"exp/F01.exp3.json", "exp"}};
    auto read = [&](std::string_view p) -> std::optional<std::string> {
      auto it = files.find(std::string(p));
      return it == files.end() ? std::nullopt : std::optional<std::string>(it->second);
    };
    const char *mapping = R"({"channels":{
      "head_tilt":["ParamAngleX","ParamAngleY","ParamAngleZ"],
      "breath":["ParamBreath"],
      "body_sway":["ParamBodyAngleZ"],
      "fringe":["ParamHairFront"],
      "sidelock":["ParamHairSide"],
      "skirt":["ParamSkirt"],
      "hand":["ParamHandDhangeL"]}})";
    auto adapter = parseAdapter(R"({"cloth_part":"Part01Body001","expressions":{"smile":"F02"}})");
    CHECK(adapter && adapter->clothPart == "Part01Body001" && adapter->expressions.size() == 1);
    auto full = inspectModel(read, "Haru.model3.json", R"({"cloth_part":"Part01Body001"})", mapping);
    CHECK(full.missingFiles.empty() && full.channels == 5 && full.usable());
    CHECK(full.fingerprint.size() == 16);
    CHECK(std::find(full.missingChannels.begin(), full.missingChannels.end(), "sidelock") != full.missingChannels.end());
    CHECK(std::find(full.notes.begin(), full.notes.end(), "adapter expression missing: smile") == full.notes.end());
    files.erase("Haru.physics3.json");
    auto broken = inspectModel(read, "Haru.model3.json", R"({"expressions":{"smile":"F02"}})", mapping);
    CHECK(std::find(broken.missingFiles.begin(), broken.missingFiles.end(), "Haru.physics3.json") !=
          broken.missingFiles.end());
    CHECK(!broken.usable());
    auto bare = std::map<std::string, std::string>{
        {"Mark.model3.json", R"({"FileReferences":{"Moc":"Mark.moc3","Textures":["Mark.1024/texture_00.png"]}})"},
        {"Mark.cdi3.json", R"({"Parameters":[{"Id":"ParamAngleX"},{"Id":"ParamAngleZ"},{"Id":"ParamBreath"}]})"},
        {"Mark.moc3", "moc"}, {"Mark.1024/texture_00.png", "png"}};
    auto bareRead = [&](std::string_view p) -> std::optional<std::string> {
      auto it = bare.find(std::string(p));
      return it == bare.end() ? std::nullopt : std::optional<std::string>(it->second);
    };
    auto mark = inspectModel(bareRead, "Mark.model3.json", "{}", mapping);
    CHECK(mark.missingFiles.empty() && mark.channels == 2 && !mark.usable());
    CHECK(std::find(mark.notes.begin(), mark.notes.end(), "no physics document") != mark.notes.end());
    auto absent = inspectModel(bareRead, "Nope.model3.json", "{}", mapping);
    CHECK(absent.missingFiles.size() == 1 && !absent.usable());
    auto registry = parseModels(R"({"models":[{"id":"haru","name":"Haru","dir":"assets/models/haru",
      "manifest":"Haru.model3.json","moc":"Haru.moc3",
      "sha256":"aa59f7a3dc3b30b23b5f10afd9192ed755e4d01dc9a12442d99b972a418e2543",
      "license":"assets/models/haru/LICENSE","adapter":{"cloth_part":"Part01Body001"}},
      {"id":"mark","name":"Mark","dir":"assets/models/builtin/mark","manifest":"Mark.model3.json",
      "moc":"Mark.moc3","sha256":"f6351429bd3d0000000000000000000000000000000000000000000000000000",
      "builtin":false}]})");
    CHECK(registry && registry->size() == 2 && (*registry)[0].adapter.clothPart == "Part01Body001");
    auto imported = parseModels(R"({"models":[{"id":"mine","name":"我的","dir":"D:\\models\\mine",
      "manifest":"a.model3.json","moc":"a.moc3","builtin":false,
      "sha256":"aa59f7a3dc3b30b23b5f10afd9192ed755e4d01dc9a12442d99b972a418e2543"}]})");
    CHECK(imported && imported->size() == 1 && !(*imported)[0].builtin);
    CHECK(!parseModels(R"({"models":[{"id":"x","name":"x","dir":"C:\\a\\..\\b","manifest":"a","moc":"a",
      "sha256":"aa59f7a3dc3b30b23b5f10afd9192ed755e4d01dc9a12442d99b972a418e2543"}]})"));
    CHECK(!parseModels(R"({"models":[{"id":"x","name":"x","dir":"../evil","manifest":"a","moc":"a",
      "sha256":"aa59f7a3dc3b30b23b5f10afd9192ed755e4d01dc9a12442d99b972a418e2543"}]})"));
    CHECK(!parseModels(R"({"models":[{"id":"x","name":"x","dir":"a/b","manifest":"a","moc":"a",
      "sha256":"short"}]})"));
    CHECK(!parseModels(R"({"models":[{"id":"x","name":"x","dir":"a","manifest":"a","moc":"a",
      "sha256":"aa59f7a3dc3b30b23b5f10afd9192ed755e4d01dc9a12442d99b972a418e2543"},
      {"id":"x","name":"y","dir":"b","manifest":"b","moc":"b",
      "sha256":"aa59f7a3dc3b30b23b5f10afd9192ed755e4d01dc9a12442d99b972a418e2543"}]})"));
  });
  std::printf("%d groups passed\n", tests);
}

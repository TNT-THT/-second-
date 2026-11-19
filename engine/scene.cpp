#include "scene.hpp"
namespace pet {
Scene::Scene(Life &l, uint32_t seed) : life(l), rng(seed) {}
int Scene::range(int a, int b) {
  return std::uniform_int_distribution<int>(a, b)(rng);
}
void Scene::selectBounds() { // Position denotes the waist, not the transparent window corner.
  auto b = w.screen;
  bounds = {b.l + 10 + 32 * scale, b.t + 10 + 75 * scale, b.r - 10 - 32 * scale, b.b - 10 - 88 * scale};
}
V Scene::bottom() const {
  return {(w.screen.l + w.screen.r) / 2, bounds.b};
}
void Scene::init(World a, sec t) {
  w = a;
  wall = t;
  desktop = w.desktop;
  selectBounds();
  walk.warp(bottom(), bounds);
  idleAt = range(60000, 120000);
  roamAt = range(60000, 120000);
  if (life.dream)
    stages.push_back(0);
  if (life.away > 48 * 3600)
    stages.push_back(1);
  if (life.neglected)
    stages.push_back(2);
  if (life.weekly)
    stages.push_back(3);
  if (life.s.returnAfterShutdown) {
    stages.push_front(4);
    life.s.returnAfterShutdown = false;
    life.save(t);
  }
  stage();
}
void Scene::action(Clip c, ms n) {
  clip = c;
  start = now;
  end = now + n;
}
void Scene::fx(Fx f, ms n) {
  effect = {f, now, n};
}
void Scene::stage() {
  if (stages.empty())
    return;
  int n = stages.front();
  stages.pop_front();
  switch (n) {
  case 0:
    talk(L"好像做了个奇怪的梦呢", 4500);
    stageAt = now + 4500;
    break;
  case 1:
    action(Clip::reunion, 5200);
    talk(L"好久不见", 3000, 2000, 150);
    if (life.away > 7 * 86400)
      sound(Sound::sob, -18);
    stageAt = now + 5700;
    break;
  case 2:
    action(Clip::miss, 10000);
    talk(L"……还以为你不要我了", 5000, 3400, 150);
    stageAt = end;
    break;
  case 4:
    talk(L"等你好久啦", 4000);
    stageAt = now + 4000;
    break;
  case 3:
    talk(L"这周辛苦啦", 4000);
    sound(Sound::weekly, -12);
    fx(Fx::heart, 2400);
    stageAt = now + 4000;
  }
}
void Scene::talk(std::wstring s, ms hold, ms delay, int c) {
  words = std::move(s);
  typing = c;
  wordsAt = now + delay;
  wordsEnd = wordsAt + hold + ms(words.size()) * c;
}
std::wstring Scene::caption() const {
  if (now < wordsAt || now >= wordsEnd)
    return {};
  return typing ? words.substr(0, size_t((now - wordsAt) / typing + 1)) : words;
}
void Scene::sound(Sound s, double db) {
  if (!mute)
    audio.push_back({s, db - (mode == Mode::work ? 10 : 0)});
}
void Scene::interrupt() {
  if (walk.going) {
    resume = walk.target;
    walked = true;
  }
  walk.stop();
}
void Scene::food(FoodResult r, bool real) {
  interrupt();
  if (r == FoodResult::no) {
    action(Clip::shake, 750);
    talk(L"这个不能吃哦");
    return;
  }
  if (r == FoodResult::io) {
    talk(L"好像做了个奇怪的梦呢");
    return;
  }
  if (r == FoodResult::full) {
    talk(L"今天吃够啦");
    return;
  }
  if (r == FoodResult::heavy) {
    action(Clip::shake, 900);
    talk(heavyLine);
    return;
  }
  action(Clip::eat, 3500);
  auto d = civil(wall);
  talk(!real                       ? L"来尝尝记忆里的味道～"
       : birthday(opt.birthday, d) ? L"生日这口，特别甜！"
       : newYear(d)                ? L"年糕软乎乎的～"
       : d.m == 10 && d.d == 31    ? L"不给糖就捣蛋！"
       : r == FoodResult::again    ? L"记得这个味道！"
                                   : L"谢谢款待～",
       3200);
}
void Scene::press(V p) {
  if (drag)
    return;
  life.wake(wall);
  if (!activePet())
    return;
  interrupt();
  left = true;
  petted = dodged = false;
  leftAt = movedAt = now;
  origin = finger = p;
  rubOffset = {};
}
void Scene::move(V p) {
  if (!left || drag)
    return;
  if (petted) {
    auto d = p - finger;
    double speed = length(d) * 1000 / std::max<ms>(1, now - movedAt);
    if (length(d) > 1 && speed > 200) {
      action(Clip::dodge, 600);
      talk(L"痒！");
      dodged = true;
      petted = false;
    } else {
      rubOffset = (p - origin) * .3;
      double n = length(rubOffset);
      if (n > 15)
        rubOffset = rubOffset * (15 / n);
    }
  }
  finger = p;
  movedAt = now;
}
void Scene::release() {
  if (!left || drag) {
    left = false;
    return;
  }
  left = false;
  if (dodged)
    return;
  if (petted) {
    petted = false;
    action(Clip::stand, 0);
    talk(L"好舒服~");
    sound(Sound::sigh, -15);
    life.stroke(wall);
    singleAt = -1;
    return;
  }
  if (now - leftAt >= 800 || length(finger - origin) > 8)
    return;
  if (singleAt >= 0 && now - lastUp < 300 && length(finger - singlePos) <= 8) {
    singleAt = -1;
    action(Clip::vsign, 1900);
    sound(Sound::chime, -14);
    if (life.s.satiety >= 80 && now >= starAt) {
      fx(Fx::star, 3000);
      starAt = now + 10000;
    }
  } else {
    singleAt = now + 300;
    lastUp = now;
    singlePos = finger;
  }
}
void Scene::hit() {
  action(Clip::jump, 500);
  sound(Sound::chime, -15);
}
void Scene::cancel() {
  left = petted = dodged = false;
  singleAt = -1;
  drag = false;
}
void Scene::dragBegin() {
  cancel();
  drag = true;
  walked = false;
  walk.warp(walk.pos, bounds);
  action(Clip::limp, 86400000);
}
void Scene::dragEnd() {
  if (!drag)
    return;
  cancel();
  action(Clip::stagger, 500);
  roamAt = now + range(60000, 120000);
}
void Scene::wheel(int steps) {
  oldScale = scale;
  goalScale = std::clamp(std::round((goalScale + steps * .1) * 10) / 10, .8, 1.5);
  scaleAt = now;
  scaleLabel = now + 3000;
}
void Scene::setMode(Mode m) {
  if (m == Mode::work)
    intents.event(working, now);
  if (m == Mode::chat)
    intents.event(curious, now);
  cancel();
  walked = false;
  mode = m;
  action(Clip::stand, 0);
  walk.stop();
  nodAt = now + 60000;
  chatAt = now + range(70000, 110000);
  roamAt = now + range(60000, 120000);
  idleAt = now + range(60000, 120000);
  if (m == Mode::chat) {
    walk.warp({bounds.l, bounds.b}, bounds);
    talk(L"好呀，我坐在这里。", 4000);
  } else if (m == Mode::work) {
    talk(L"我陪着你。", 3500);
  } else
    talk(L"那我去散散步～", 2200);
}
void Scene::reset() {
  smartStop();
  setMode(Mode::free);
  scale = oldScale = goalScale = 1;
  scaleLabel = 0;
  selectBounds();
  walk.warp(bottom(), bounds);
  words.clear();
  effect = {};
  stages.clear();
  lateWork = false;
}
void Scene::respond() {
  if (mode == Mode::chat && replies < 5 && !caption().empty()) {
    ++replies;
    talk(L"嗯？", 2000);
    blinkAt = now;
  }
}
void Scene::calm() {
  if (!opt.input || now < calmAt || !activePet() || life.s.satiety == 0)
    return;
  intents.event(needsComfort, now);
  calmAt = now + 900000;
  walk.aim(w.cursor + V{0, 30}, bounds);
  action(Clip::calm, 6000);
  talk(L"深呼吸~", 6000);
}
void Scene::accepted(const std::string &id) {
  if (!opt.luogu || !w.browser || !w.luogu || !activePet() || life.s.satiety == 0 || id.empty() ||
      id.size() > 32 ||
      id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != id.npos)
    return;
  for (auto i = wins.begin(); i != wins.end();)
    if (i->second <= now)
      i = wins.erase(i);
    else
      ++i;
  if (wins.count(id) || wins.size() >= 256)
    return;
  intents.event(success, now);
  wins[id] = now + 1800000;
  walk.aim({(w.front.l + w.front.r) / 2, w.front.t + 72}, bounds);
  action(Clip::congrats, 2000);
  fx(Fx::flowers, 1200);
  talk(L"恭喜！", 2000);
  sound(Sound::chime, -12);
}
void Scene::tick(ms dt, sec time, World world) {
  if (dt <= 0)
    return;
  auto before = now;
  now += dt;
  intents.tick(now);
  wall = time;
  w = world;
  auto d = civil(wall);
  life.tick(dt, true, mode == Mode::chat, wall);
  double t = std::min(100., double(dt)) / 1000;
  scale = oldScale + (goalScale - oldScale) * ease(double(now - scaleAt) / 300.);
  selectBounds();
  if (life.dream) {
    life.dream = false;
    if (caption() != L"好像做了个奇怪的梦呢")
      talk(L"好像做了个奇怪的梦呢", 4500);
  }
  if (!stages.empty() && now >= stageAt && !drag)
    stage();
  if (now > blinkAt + 200) {
    blinkCycle = range(3334, 5000);
    blinkAt = now + blinkCycle;
  }
  if (left && !petted && !dodged && now - leftAt >= 800) {
    petted = true;
    singleAt = -1;
    action(Clip::rub, 86400000);
  }
  if (singleAt >= 0 && now >= singleAt && !left) {
    singleAt = -1;
    hit();
  }
  if (clip == Clip::eat && before < start + 3300 && now >= start + 3300) {
    sound(Sound::satisfied, -12);
    if (newYear(d))
      sound(Sound::cracker, -18);
  }
  if (!busy()) {
    clip = Clip::stand;
    if (walked && !left && !drag && mode == Mode::free) {
      walk.aim(bounds.clip(resume), bounds);
      walked = false;
    }
  }
  double targetBlush = (petted || clip == Clip::eat || life.s.satiety <= 20 && life.s.satiety > 0) ? .4 : 0.;
  blush += std::clamp(targetBlush - blush, -.4 * t / 1.2, .4 * t / .8);
  auto eye = w.cursor - walk.pos;
  double interp = ease(t / .2);
  gx += (std::clamp(eye.x / 8., -20., 20.) - gx) * interp;
  gy += (std::clamp(eye.y / 8., -20., 20.) - gy) * interp;
  lateWork = late(d) && (w.editor || w.luogu && w.browser);
  if (birthday(opt.birthday, d) && celebrationDay != day(wall) && now >= stageAt) {
    celebrationDay = day(wall);
    sound(Sound::birthday, -8);
    fx(Fx::flowers, 1200);
    talk(L"生日快乐！", 5000);
  }
  if (drag)
    return;
  if (life.s.satiety == 0) {
    walk.stop();
    walk.step(t, now, bounds, w, 0, true);
    return;
  }
  bool calmWalk = busy() && (clip == Clip::calm || clip == Clip::congrats || clip == Clip::reunion);
  if (clip == Clip::reunion && before < start + 2000 && now >= start + 2000)
    walk.aim({(w.screen.l + w.screen.r) * .5, (w.screen.t + w.screen.b) * .5}, bounds);
  if (mode == Mode::work && !left) {
    V target = w.window ? V{w.front.r + 10 + 32 * scale, std::clamp(w.front.b - 100., w.front.t, w.front.b)}
                        : bottom();
    target = bounds.clip(target);
    if (length(target - walk.pos) > 1)
      walk.aim(target, bounds);
    if (!busy() && now >= nodAt) {
      nodAt = now + 60000;
      action(Clip::nod, 900);
    }
  } else if (mode == Mode::chat) {
    if (!busy() && now >= chatAt) {
      chatAt = now + range(70000, 110000);
      if (!lines.empty())
        talk(lines[size_t(rng()) % lines.size()], 6000);
    }
  } else if (lateWork && !busy() && !left) {
    V target = bounds.clip({w.front.l + 35, w.front.b - 88});
    if (length(target - walk.pos) > 1)
      walk.aim(target, bounds);
    if (now >= nightAt) {
      nightAt = now + 1800000;
      talk(L"别熬太晚哦");
    }
  } else if (!busy() && !left && !walk.going && now >= walk.rest) {
    if (now >= roamAt) {
      roamAt = now + range(60000, 120000);
      walk.aim(chooseTarget(w, bounds, rng), bounds);
    } else if (now >= idleAt) {
      idleAt = now + range(60000, 120000);
      int first = range(0, 2);
      for (int i = 0; i < 3; i++) {
        int n = (first + i) % 3;
        if (now >= cool[n]) {
          cool[n] = now + 300000;
          action(std::array{Clip::sway, Clip::sit, Clip::back}[n], std::array{2000, 4000, 3000}[n]);
          break;
        }
      }
    }
  }
  if (life.s.satiety <= 20 && !busy() && now >= bellyAt) {
    bellyAt = now + 60000;
    action(Clip::belly, 1200);
    sound(Sound::sigh, -15);
  }
  if (mode == Mode::free && w.desktop && !desktop) {
    deskAt = now;
    extra = false;
    walk.aim({(w.screen.l + w.screen.r) / 2, (w.screen.t + w.screen.b) / 2}, bounds);
    action(Clip::stretch, 2000);
    talk(L"终于看我啦！", 5000);
  }
  if (w.desktop && deskAt > 0 && !extra && now - deskAt >= 30000 && mode == Mode::free && !busy()) {
    extra = true;
    action(Clip::spin, 4000);
  }
  if (deskAt && now - deskAt >= 5000 && before - deskAt < 5000 && mode == Mode::free)
    walk.aim(bottom(), bounds);
  if (!w.desktop)
    deskAt = 0;
  desktop = w.desktop;
  double factor = (lowBattery ? .7 : 1.) * (rain ? .85 : 1.) * (shallow ? .40 : 1.) *
                  (night(d) || w.dark ? .7 : 1.) * (life.s.satiety <= 20 ? .7 : 1.) *
                  (clip == Clip::reunion ? .65 : 1.);
  walk.step(t, now, bounds, w, factor, calmWalk || mode != Mode::free || lateWork);
  double drive = std::clamp(-walk.lean * .8 - walk.speed * .07, -25., 25.);
  hairV = (hairV + (drive - hair) * .8 * t * 60) * std::pow(.6, t * 10);
  hair = std::clamp(hair + hairV * t * (1 - .3), -25., 25.);
  if (walk.speed > 15 && now >= stepAt) {
    stepAt = now + 540;
    sound(Sound::steps, night(d) || w.dark ? -34 : -28);
  }
}
Pose Scene::pose() const {
  Pose p;
  auto d = civil(wall);
  double s = double(now - start) / 1000, phase = double(now) / 1000;
  bool sleep = life.s.satiety == 0, hungry = life.s.satiety <= 20 && !sleep;
  p.spring = newYear(d);
  p.halloween = d.m == 10 && d.d == 31;
  p.breath = .5 + .5 * std::sin(phase * 2 * pi *
                                (clip == Clip::calm     ? 24
                                 : sleep                ? 2
                                 : life.s.satiety >= 80 ? 18
                                                        : 14) /
                                60);
  p.eye = sleep ? .12 : 1.;
  if (now >= blinkAt && now < blinkAt + 200)
    p.eye *= 1 - std::sin(pi * (now - blinkAt) / 200.);
  p.gx = gx;
  p.gy = hungry ? 15 : gy;
  p.blush = blush;
  p.lean = walk.lean;
  p.hair = hair;
  p.wind = 10 * std::sin(phase * .8);
  p.light = (w.dark ? 1.10 : .95) * (lateWork ? .70 : 1.) * (life.s.satiety >= 80 ? 1.035 : 1.);
  p.highlights = night(d) || w.dark ? .85 : 1.;
  p.saturation = sleep ? .3 : 1.;
  p.sit = mode == Mode::chat ? 1 : 0;
  p.step = std::sin(phase * 2 * pi * 1.85) * walk.speed / 80;
  p.skirt = walk.speed > 65 ? 15 : walk.speed > 2 ? 8 : 0;
  if (left)
    p.lean += 5;
  switch (clip) {
  case Clip::eat: {
    double amp = 1 - ease((s - 3.3) / .2);
    p.cup = amp;
    p.eye *= 1 + .2 * amp;
    p.smile = amp;
    if (s >= .3 && s < 3.3)
      p.jaw = 4 * (1 - std::cos(2 * pi * 2 * (s - .3)));
    break;
  }
  case Clip::shake:
    p.lean += 8 * std::sin(s * 4 * pi);
    break;
  case Clip::jump:
    p.jump = -30 * std::sin(pi * std::min(s / .3, 1.));
    p.wave = std::sin(pi * std::min(s / .5, 1.));
    break;
  case Clip::vsign:
    p.offset = -20 * (s < .2 ? ease(s / .2) : 1 - ease((s - .2) / .2));
    p.jump = -12 * std::sin(pi * std::min(1., s / .4));
    p.vsign = s >= .4 ? 1 : 0;
    break;
  case Clip::rub:
    p.eye = 0;
    p.rub = rubOffset.x;
    p.jump += rubOffset.y;
    break;
  case Clip::dodge:
    p.offset = 15 * std::sin(pi * std::min(1., s / .6));
    p.surprise = 1;
    break;
  case Clip::limp:
    p.limp = 1;
    p.skirt = 15;
    break;
  case Clip::stagger:
    p.lean += 12 * std::sin(4 * pi * s) * (1 - ease(s / .5));
    break;
  case Clip::sway:
    p.lean += 15 * std::sin(pi * s);
    break;
  case Clip::sit:
    p.sit = 1;
    p.hand = 15;
    p.gx = p.gy = 0;
    break;
  case Clip::back:
    p.back = std::sin(pi * std::min(1., s / 3));
    break;
  case Clip::belly:
    p.hand = 40 * std::sin(pi * std::min(1., s / 1.2));
    break;
  case Clip::congrats:
    p.spin = 360 * ease(s / 1.2);
    break;
  case Clip::nod:
    p.nod = 8 * std::sin(pi * std::min(1., s / .9));
    break;
  case Clip::stretch:
    p.lean -= 15 * std::sin(pi * std::min(1., s / 2));
    break;
  case Clip::spin:
    p.spin = 1080 * ease(s / 4);
    break;
  case Clip::reunion:
  case Clip::miss:
    p.back = 1 - ease((s - (clip == Clip::miss ? 3 : 2)) / .4);
    break;
  default:
    break;
  }
  p.track = clip == Clip::eat || clip == Clip::rub          ? 1
            : clip == Clip::vsign || clip == Clip::congrats ? 3
            : clip == Clip::jump || clip == Clip::belly     ? 2
                                                            : 0;
  p.trackTime = p.track ? now - start : now;
  if (shallow && !sleep) {
    p.breath = .5 + .5 * std::sin(phase * 2 * pi / 6);
    p.gx = 5 * std::sin(phase * .4);
    p.eye *= .6;
  }
  if (opt.ime && imeKnown && clip == Clip::stand && !sleep) {
    p.sit = imeChinese ? 1 : 0;
    p.hand = imeChinese ? 15 : 0;
  }
  if (opt.meter && !sleep) {
    if (beat.bpm > 0)
      p.nod += 8 * std::sin(2 * pi * phase * beat.bpm / 60);
    if (beat.muted && clip == Clip::stand) {
      p.eye = 0;
      p.hand = 28;
    }
    if (beat.peak > .8) {
      p.lean *= .7;
      p.jump *= .7;
      p.step *= .7;
      p.nod *= .7;
    }
  }
  p.rain = opt.weather && rain;
  if (opt.weather && sun)
    p.highlights *= 1.08;
  p.hue = warm * 5;
  p.highlightSat = 1 - .1 * cold;
  p.tag = intents.tag;
  p.gazeGain = style.gaze;
  if (intents.tag != Tag::neutral) {
    auto tag = intents.tag;
    auto e = envelope(tag, now - intents.since, capability, intents.compose ? style : Style{1, 1, 1});
    p.expression = e;
    double a = e.weight;
    p.jump += e.micro;
    if (intents.compose && e.tier < 3) {
      // No arm/physics dependency: rhythm -> single-channel breath -> face -> full rig.
      p.light *= 1 + e.beat * .035;
      if (e.tier >= 1)
        p.breath = .5 + e.beat * .32;
      if (e.tier >= 2) {
        p.eye *= 1 - .13 * a;
        p.gx += 4 * e.beat;
        p.smile = .35 * a;
      }
      return p;
    }
    if (tag == Tag::celebrate) {
      p.eye += (1 - p.eye) * a;
      p.jaw = std::max(p.jaw, 6.4 * a);
      p.lean += 15 * a;
      p.track = 3;
      p.trackTime = now - intents.since;
    } else if (tag == Tag::comfort) {
      p.eyesSmile = .6 * a;
      p.bodyYaw = -5 * a;
      p.breath = .4 + .35 * std::sin(phase * 2 * pi / 5);
    } else if (tag == Tag::focus) {
      p.gx *= .4;
      p.gy *= .4;
      p.nod += 2 * e.beat;
      p.breath = .55 + .35 * std::sin(phase * 2 * pi * 16 / 60);
    } else if (tag == Tag::curiosity) {
      p.lean += 10 * a;
      p.hand = 15 * a;
      p.sit = a;
    } else if (tag == Tag::rest) {
      p.eye *= 1 - .3 * a;
      p.smile = -.3 * a;
      p.light *= 1 - .3 * a;
    }
  }
  return p;
}
void Scene::smartStop() {
  intents.cancel();
  intents.recognize = intents.compose = false;
  effect = {};
}
void Scene::observeDoodle(int i) {
  static constexpr const wchar_t *feedback[] = {
      L"好可爱！",           L"这个是什么呀？",       L"我喜欢这一笔。",
      L"像在悄悄说话。",     L"原来你是这样看的呀。", L"能再看一会儿吗？",
      L"黑和白也很热闹呢。", L"这一角真有趣！",       L"谢谢你画给我。",
      L"我会记得这个形状。"};
  interrupt();
  action(Clip::sit, 3000);
  talk(feedback[unsigned(i) % 10], 4500, 3000);
  intents.event(curious, now);
}
void Scene::stretch() {
  action(Clip::stretch, 2000);
  talk(L"早呀，我醒啦。", 3000);
}
void Scene::farewell() {
  walk.aim(V{(w.screen.l + w.screen.r) * .5, (w.screen.t + w.screen.b) * .5}, bounds);
  action(Clip::jump, 1700);
  talk(L"待会见~", 6000);
}
void Scene::memo(bool ac, bool wa) {
  if (ac) {
    intents.event(success, now);
    action(Clip::vsign, 1900);
    talk(L"AC！一起比个耶！");
  } else if (wa) {
    intents.event(needsComfort, now);
    action(Clip::rub, 1500);
    talk(L"WA 也没关系，再试一次。");
  } else
    talk(L"我读过这条备忘啦。");
}

} // namespace pet

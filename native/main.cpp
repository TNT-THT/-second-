#include "bridge.hpp"
#include "drop.hpp"
#include "feed.hpp"
#include "json.hpp"
#include "environment.hpp"
#include "workbench.hpp"
#include "memo.hpp"
#include "view.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wtsapi32.h>
#include <dwmapi.h>
#include <ctime>
#include <cwctype>
#include <algorithm>
#ifdef PET_DEV
#include <psapi.h>
#endif
namespace pet {
constexpr wchar_t className[] = L"DeskCompanion.Native.2";
static V cursor() {
  POINT p{};
  GetCursorPos(&p);
  return {double(p.x), double(p.y)};
}
static sec wallTime() {
  return static_cast<sec>(std::time(nullptr));
}
static V center(Box b) {
  return {(b.l + b.r) / 2, (b.t + b.b) / 2};
}
static Box rect(RECT r) {
  return {double(r.left), double(r.top), double(r.right), double(r.bottom)};
}
static std::wstring pick(HWND win) {
  IFileOpenDialog *d = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d))))
    return {};
  DWORD f = 0;
  d->GetOptions(&f);
  d->SetOptions(f | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_STRICTFILETYPES |
                FOS_NOCHANGEDIR | FOS_DONTADDTORECENT | FOS_NODEREFERENCELINKS);
  COMDLG_FILTERSPEC filter[] = {{L"可以吃的文件", L"*.txt;*.png;*.cpp;*.json"}};
  d->SetFileTypes(1, filter);
  d->SetTitle(L"投喂 · 只看文件属性，不打开内容");
  std::wstring p;
  if (SUCCEEDED(d->Show(win))) {
    IShellItem *i = nullptr;
    if (SUCCEEDED(d->GetResult(&i))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(i->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        p = path;
        CoTaskMemFree(path);
      }
      i->Release();
    }
  }
  d->Release();
  return p;
}
static std::wstring pickModel(HWND win) {
  IFileOpenDialog *d = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d))))
    return {};
  DWORD f = 0;
  d->GetOptions(&f);
  d->SetOptions(f | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_STRICTFILETYPES |
                FOS_NOCHANGEDIR | FOS_DONTADDTORECENT | FOS_NODEREFERENCELINKS);
  COMDLG_FILTERSPEC filter[] = {{L"Live2D 模型描述", L"*.model3.json"}};
  d->SetFileTypes(1, filter);
  d->SetTitle(L"导入模型 · 只读取所选目录里的描述与资源");
  std::wstring p;
  if (SUCCEEDED(d->Show(win))) {
    IShellItem *i = nullptr;
    if (SUCCEEDED(d->GetResult(&i))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(i->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        p = path;
        CoTaskMemFree(path);
      }
      i->Release();
    }
  }
  d->Release();
  return p;
}
struct App {
  HWND hwnd = nullptr;
  Vault files{Vault::exeDir()};
  Life life{files};
  Scene s{life, DWORD(GetTickCount64() ^ GetCurrentProcessId())};
  std::unique_ptr<View> view;
  Audio audio{files};
  Bridge bridge;
  Environment env{files};
  Memo memo{files};
  std::unique_ptr<Workbench> bench;
  std::vector<ModelEntry> models;
  std::string modelId = "haru";
  FeedLimits limits;
  bool recycleConsent = false, askedRecycle = false;
  IDropTarget *dropTarget = nullptr;
  std::wstring quickNotice;
  bool voiceHot = false, voiceHold = false;
  ms voiceAt = 0;
  void pure() {
    memo.cancel();
    voiceHold = false;
    bool was = s.opt.intent || s.opt.feedback;
    s.reset();
    s.opt.intent = s.opt.feedback = false;
    if (was)
      files.note(Note::preferences, encode(s.opt));
  }
  bool choose(const Options &o) {
    if (!files.note(Note::preferences, encode(o)))
      return false;
    bool sig = o.intent != s.opt.intent || o.feedback != s.opt.feedback;
    if (sig)
      s.intents.cancel();
    s.opt = o;
    s.intents.recognize = s.trusted && o.intent;
    s.intents.compose = s.trusted && o.feedback;
    sensors();
    return true;
  }

  Rates rates;
  HWND observed = nullptr;
  DWORD observedPid = 0;
  bool editor = false, browser = false;
  ms hereUntil = -1;
  bool raw = false, locked = false, off = false, suspended = false, modal = false, escape = false,
       right = false, bubbleClick = false, inFeed = false;
  V down{}, grab{};
  ms rightAt = 0, prev = 0, lastWorld = -1000, menuAt = -1;
  int interval = 0;
  HPOWERNOTIFY power = nullptr;
  World world;
#ifdef PET_DEV
  bool debug = false;
  double fps = 0;
  ms reloadAt = 0;
  size_t kib = 0;
#endif
  bool paused() const {
    return locked || off || suspended;
  }
  void reschedule(int n) {
    if (interval != n) {
      SetTimer(hwnd, 1, UINT(n), nullptr);
      interval = n;
    }
  }
  void sensors() {
    env.configure(s.opt, paused());
    bool canVoice = s.opt.voice && Memo::supported() && !paused();
    if (canVoice && !voiceHot) {
      voiceHot = RegisterHotKey(hwnd, 4, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'V') != 0;
      if (voiceHot)
        memo.arm();
    } else if (!canVoice) {
      if (voiceHot)
        UnregisterHotKey(hwnd, 4);
      voiceHot = voiceHold = false;
      memo.cancel();
    }
    bool need = s.opt.input && !paused();
    if (raw != need) {
      RAWINPUTDEVICE ds[2] = {
          {1, 2, need ? DWORD(RIDEV_INPUTSINK) : DWORD(RIDEV_REMOVE), need ? hwnd : nullptr},
          {1, 6, need ? DWORD(RIDEV_INPUTSINK) : DWORD(RIDEV_REMOVE), need ? hwnd : nullptr}};
      if (RegisterRawInputDevices(ds, 2, sizeof(ds[0])))
        raw = need;
      rates.reset();
    }
    if (s.opt.luogu && !paused())
      bridge.start();
    else
      bridge.stop();
  }
  void rawEvent(HRAWINPUT token) {
    if (!raw || !s.opt.input || paused())
      return;
    RAWINPUT value{};
    UINT size = sizeof value;
    if (GetRawInputData(token, RID_INPUT, &value, &size, sizeof(RAWINPUTHEADER)) == UINT(-1))
      return;
    if (value.header.dwType == RIM_TYPEKEYBOARD) {
      if (!(value.data.keyboard.Flags & RI_KEY_BREAK))
        rates.count(true);
    } else if (value.header.dwType == RIM_TYPEMOUSE) {
      const USHORT flags = value.data.mouse.usButtonFlags;
      for (auto bit : {RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_DOWN,
                       RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_5_DOWN})
        if (flags & bit)
          rates.count(false);
    }
  }
  void foreground(bool force) {
    HWND fg = GetForegroundWindow();
    bool self = fg == hwnd;
    DWORD pid = 0;
    if (fg)
      GetWindowThreadProcessId(fg, &pid);
    POINT mp{LONG(s.walk.pos.x), LONG(s.walk.pos.y)};
    auto mon = MonitorFromPoint(mp, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    world.screen = rect(mi.rcWork);
    if (!self && (force || fg != observed || pid != observedPid)) {
      observed = fg;
      observedPid = pid;
      editor = browser = false;
      if (fg && pid && pid != GetCurrentProcessId()) {
        Handle p(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        wchar_t name[2048];
        DWORD count = 2048;
        if (p && QueryFullProcessImageNameW(p.h, 0, name, &count)) {
          std::wstring exe = name;
          exe = exe.substr(exe.find_last_of(L"\\/") + 1);
          for (auto &c : exe)
            if (c >= L'A' && c <= L'Z')
              c += 32;
          for (auto b : {L"chrome.exe", L"msedge.exe", L"firefox.exe", L"brave.exe"})
            browser |= exe == b;
          for (auto e : {L"code.exe", L"code - insiders.exe", L"devenv.exe", L"notepad.exe", L"notepad++.exe",
                         L"clion64.exe", L"idea64.exe", L"pycharm64.exe", L"sublime_text.exe", L"devcpp.exe",
                         L"cursor.exe"})
            editor |= exe == e;
        }
      }
    }
    if (self)
      fg = observed;
    wchar_t cls[96]{};
    if (fg)
      GetClassNameW(fg, cls, 96);
    world.desktop =
        fg && (fg == GetShellWindow() || wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0);
    RECT b{};
    world.window = fg && !world.desktop && !IsIconic(fg) && GetWindowRect(fg, &b) && b.right > b.left &&
                   b.bottom > b.top;
    world.front = world.window ? rect(b) : world.screen;
    world.editor = world.window && editor;
    world.browser = world.window && browser;
    world.luogu = s.opt.luogu && world.browser && s.now < hereUntil;
    world.cursor = cursor();
    COLORREF c = GetSysColor(COLOR_WINDOW);
    BOOL dark = FALSE;
    if (fg)
      DwmGetWindowAttribute(fg, 20, &dark, sizeof(dark));
    world.dark = dark || (GetRValue(c) * .21 + GetGValue(c) * .72 + GetBValue(c) * .07 < 115);
  }
  void init(HWND win) {
    hwnd = win;
    auto t = wallTime();
    files.recover(life, t);
    auto cfg = files.options();
    life.dream |= !cfg;
    s.opt = cfg.value_or(Options{});
    for (auto &line : files.notes(Note::preferences, 1)) {
      auto saved = decodeOptions(line);
      if (saved && cfg)
        s.opt = *saved;
      else {
        s.opt = {};
        life.dream = true;
      }
    }
    // An edited config alone is not affirmative smart-mode or microphone consent.
    auto prior = files.notes(Note::preferences, 1);
    if (prior.empty())
      s.opt.intent = s.opt.feedback = s.opt.meter = s.opt.ime = s.opt.gamma = s.opt.voice = false;
    for (auto &line : files.notes(Note::style, 1)) {
      auto style = policy(line);
      if (style)
        s.style = *style;
    }

    auto b = files.asset(L"chat.txt", 32 * 1024);
    std::string line;
    for (char ch : b) {
      if (ch == '\n') {
        if (!line.empty())
          s.lines.push_back(wide(line));
        line.clear();
      } else if (ch != '\r')
        line += ch;
    }
    if (!line.empty())
      s.lines.push_back(wide(line));
    foreground(true);
    s.init(world, t);
    view = std::make_unique<View>(hwnd, files);
    loadLimits();
    loadModels();
    installDrop();
    if (!modelId.empty())
      switchModel(modelId, false);
    WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
    power = RegisterPowerSettingNotification(hwnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);
    bool keys = RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'M') != 0;
    keys &= RegisterHotKey(hwnd, 2, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'F') != 0;
    if (!keys)
      s.talk(L"快捷键被借走啦，右键也可以哦。", 5000);
#ifdef PET_DEV
    RegisterHotKey(hwnd, 3, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'D');
#endif
    sensors();
    prev = GetTickCount64();
    frame();
    s.intents.recognize = s.trusted && s.opt.intent;
    s.intents.compose = s.trusted && s.opt.feedback;
    bench = std::make_unique<Workbench>(
        s, files, *view, [this](const Options &o) { return choose(o); }, [this] { return env.report(); },
        [] { return Memo::supported(); });
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    reschedule(40);
  }
  void frame() {
    if (!view)
      return;
    view->show(s, audio.mouth()
#ifdef PET_DEV
                      ,
               debug, fps, kib
#endif
    );
    for (auto cue : s.audio)
      audio.play(cue);
    s.audio.clear();
  }
  void pauseChanged() {
    if (paused()) {
      life.save(wallTime());
      audio.stop();
      s.cancel();
      right = bubbleClick = false;
      ReleaseCapture();
      KillTimer(hwnd, 1);
      interval = 0;
      ShowWindow(hwnd, SW_HIDE);
    } else {
      prev = GetTickCount64();
      world.cursor = cursor();
      frame();
      ShowWindow(hwnd, SW_SHOWNOACTIVATE);
      reschedule(40);
    }
    hereUntil = -1;
    sensors();
  }
  void onTimer() {
    if (paused() || !view)
      return;
    ms rt = GetTickCount64();
    ms dt = rt - prev;
    prev = rt;
    if (dt <= 0)
      return;
    bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8001) != 0;
    if (!modal && esc && !escape) {
      pure();
      audio.stop();
      right = false;
      ReleaseCapture();
    }
    escape = esc;
    if (rt - lastWorld >= 250 || s.drag) {
      foreground(false);
      lastWorld = rt;
    } else
      world.cursor = cursor();
    if (!IsWindowVisible(hwnd) && !modal)
      ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    if (dt > 2000) {
      rates.reset();
      return;
    }
    if (!GetForegroundWindow() || modal) {
      return;
    }
    env.update(s, GetForegroundWindow(), rt);
    if (voiceHot && (voiceHold || memo.recording())) {
      bool held =
          GetAsyncKeyState('V') < 0 && GetAsyncKeyState(VK_CONTROL) < 0 && GetAsyncKeyState(VK_SHIFT) < 0;
      if (voiceHold && !held)
        voiceHold = false;
      if (voiceHold && rt - voiceAt >= 350) {
        voiceHold = false;
        if (memo.begin())
          s.talk(L"在听，松开就好。", 10000);
        else
          s.talk(L"离线语音还未就绪，没有录音。");
      }
      memo.tick(s, held);
    } else if (voiceHot)
      memo.tick(s, false);
    auto events = bridge.drain();
    if (s.opt.luogu && !events.empty()) {
      hereUntil = s.now + 6500;
      world.luogu = world.browser;
    }
    s.tick(dt, wallTime(), world);
    if (s.lateWork && !life.s.guardian) {
      life.s.guardian = true;
      life.save(wallTime());
      s.intents.event(lateHour, s.now);
    }
    for (auto &e : events)
      if (!e.id.empty())
        s.accepted(e.id);
    if (raw && rates.advance(rt / 1000))
      s.calm();
    if (right && !s.drag && s.now - rightAt >= 450 && length(world.cursor - down) > 3) {
      s.dragBegin();
    }
    if (s.drag) {
      s.walk.warp(world.cursor - grab, s.bounds);
    }
    if (menuAt >= 0 && s.now >= menuAt) {
      menuAt = -1;
      menu(false);
    }
#ifdef PET_DEV
    fps = fps * .92 + .08 * (1000. / dt);
    if (rt >= reloadAt) {
      reloadAt = rt + 400;
      if (debug) {
        PROCESS_MEMORY_COUNTERS mem{sizeof(mem)};
        GetProcessMemoryInfo(GetCurrentProcess(), &mem, sizeof(mem));
        kib = mem.WorkingSetSize / 1024;
        auto o = files.options();
        auto opt = o.value_or(Options{});
        if (!o)
          life.dream = true;
        if (opt.input != s.opt.input || opt.luogu != s.opt.luogu || opt.birthday != s.opt.birthday) {
          s.opt = opt;
          sensors();
        }
      }
    }
#endif
    frame();
    reschedule(s.drag || right || s.activeVisual() ? 33 : life.s.satiety == 0 ? 180 : 80);
  }
  static std::string entryLine(const ModelEntry &e) {
    nlohmann::json j;
    j["kind"] = "entry";
    j["id"] = e.id;
    j["name"] = e.name;
    j["dir"] = e.dir;
    j["manifest"] = e.manifest;
    j["moc"] = e.moc;
    j["sha256"] = e.sha;
    j["builtin"] = e.builtin;
    nlohmann::json a;
    a["cloth_part"] = e.adapter.clothPart;
    a["expressions"] = nlohmann::json::object();
    for (auto &[k, v] : e.adapter.expressions)
      a["expressions"][k] = v;
    if (!e.adapter.tracks.empty())
      a["tracks"] = e.adapter.tracks;
    j["adapter"] = a;
    return j.dump();
  }
  void loadModels() {
    models.clear();
    auto reg = parseModels(files.setting(L"builtin_models.json", 128 * 1024));
    if (reg)
      models = *reg;
    else if (!files.setting(L"builtin_models.json", 128 * 1024).empty())
      life.dream = true;
    for (auto &line : files.notes(Note::models, 64)) {
      nlohmann::json j = nlohmann::json::parse(line, nullptr, false);
      if (j.is_discarded() || !j.is_object())
        continue;
      auto kind = j.value("kind", std::string());
      if (kind == "entry") {
        auto one = parseModels("{\"models\":[" + line + "]}");
        if (one && one->size() == 1) {
          auto exists = std::any_of(models.begin(), models.end(),
                                    [&](const ModelEntry &m) { return m.id == one->front().id; });
          if (!exists)
            models.push_back(one->front());
        }
      } else if (kind == "active") {
        modelId = j.value("id", modelId);
      } else if (kind == "consent") {
        recycleConsent = j.value("recycle", false);
        askedRecycle = true;
      }
    }
    if (models.empty()) {
      life.dream = true;
      modelId.clear();
    }
  }
  void loadLimits() {
    auto raw = files.feedLimits();
    auto parsed = parseFeedLimits(raw);
    if (!raw.empty() && !parsed)
      life.dream = true; // edited but unreadable: fall back to the documented default and stay quiet
    limits = parsed.value_or(FeedLimits{});
    s.heavyLine = wide(limits.heavyLine);
  }
  bool switchModel(const std::string &id, bool persist = true) {
    if (!view)
      return false;
    const ModelEntry *entry = nullptr;
    for (auto &m : models)
      if (m.id == id)
        entry = &m;
    if (!entry)
      return false;
    auto started = GetTickCount64();
    if (!view->loadModel(files, *entry)) {
      ModelEntry fallback = haruModelEntry();
      for (auto &m : models)
        if (m.id == "haru")
          fallback = m;
      view->loadModel(files, fallback);
      s.trusted = view->hasActor();
      s.smartStop();
      s.talk(L"这个模型好像缺了点什么…", 4000);
      return false;
    }
    modelId = id;
    s.trusted = view->hasActor();
    s.capability = 0; // measured again from the new rig on the next frame
    s.cancel();       // no clip or intent survives the swap
    s.intents.recognize = s.trusted && s.opt.intent;
    s.intents.compose = s.trusted && s.opt.feedback;
    if (persist)
      files.note(Note::models, "{\"kind\":\"active\",\"id\":\"" + id + "\"}");
    if (bench) {
      auto took = GetTickCount64() - started;
      bench->switched(entry->name, took);
    }
    prev = GetTickCount64();
    frame();
    return true;
  }
  void importModel(const std::wstring &path) {
    auto at = path.find_last_of(L'\\');
    if (at == std::wstring::npos) {
      quickNotice = L"这个位置读不了呢";
      return;
    }
    auto dir = path.substr(0, at);
    auto leaf = utf8(path.substr(at + 1));
    auto previous = modelId;
    if (!files.useModel(utf8(dir)) || leaf.empty()) {
      quickNotice = L"这个位置读不了呢";
      if (!previous.empty())
        switchModel(previous, false);
      return;
    }
    auto descriptor = files.modelAsset(leaf, 2 * 1024 * 1024);
    if (descriptor.empty()) {
      quickNotice = L"读不到 model3.json";
      switchModel(previous, false);
      return;
    }
    std::string text(descriptor.begin(), descriptor.end());
    nlohmann::json man;
    try {
      man = nlohmann::json::parse(text);
    } catch (...) {
      quickNotice = L"这份描述读不懂呢";
      switchModel(previous, false);
      return;
    }
    auto moc = man.value("FileReferences", nlohmann::json::object()).value("Moc", std::string());
    if (moc.empty()) {
      quickNotice = L"缺少 .moc3";
      switchModel(previous, false);
      return;
    }
    auto adapterText = files.modelHas("model_adapter.json")
                           ? std::string([&] {
                               auto b = files.modelAsset("model_adapter.json", 64 * 1024);
                               return std::string(b.begin(), b.end());
                             }())
                           : std::string("{}");
    auto reader = [&](std::string_view rel) -> std::optional<std::string> {
      auto b = files.modelAsset(rel, 2 * 1024 * 1024);
      if (b.empty())
        return std::nullopt;
      return std::string(b.begin(), b.end());
    };
    auto report = inspectModel(reader, leaf, adapterText, files.setting(L"intent_mapping.json"));
    if (!report.missingFiles.empty()) {
      std::wstring missing;
      for (size_t i = 0; i < report.missingFiles.size() && i < 3; ++i)
        missing += (i ? L"、" : L"") + wide(report.missingFiles[i]);
      quickNotice = L"缺少 " + missing;
      switchModel(previous, false);
      return;
    }
    ModelEntry e;
    e.id = leaf;
    for (auto &c : e.id)
      if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-')
        c = '_';
    e.name = leaf;
    for (size_t n = 2;; ++n) {
      bool clash = std::any_of(models.begin(), models.end(), [&](const ModelEntry &m) { return m.id == e.id; });
      if (!clash)
        break;
      e.id = leaf + "-" + std::to_string(n);
    }
    e.dir = utf8(dir);
    e.manifest = leaf;
    e.moc = moc;
    auto mocBytes = files.modelAsset(moc, 8 * 1024 * 1024);
    if (mocBytes.empty()) {
      quickNotice = L"缺少 " + wide(moc);
      switchModel(previous, false);
      return;
    }
    e.sha = sha256({(char *)mocBytes.data(), mocBytes.size()});
    e.builtin = false;
    e.adapter = parseAdapter(adapterText).value_or(ModelAdapter{});
    models.push_back(e);
    if (!files.note(Note::models, entryLine(e))) {
      models.pop_back();
      quickNotice = L"记不住这个模型呢";
      switchModel(previous, false);
      return;
    }
    if (switchModel(e.id)) {
      quickNotice = L"接上了 " + wide(e.name) + L"（" + std::to_wstring(report.channels) + L" 个通道）";
      files.note(Note::models, "{\"kind\":\"active\",\"id\":\"" + e.id + "\"}");
    }
  }
  void consent() {
    if (askedRecycle || !limits.recycle)
      return;
    // Never a modal dialog: the desktop pet must not steal focus, and an automated probe would hang on
    // it. A one-time bubble points at the right-click switch; until it is on we keep metadata only.
    askedRecycle = true;
    s.talk(L"想让我把吃过的那一份放进回收站吗？右键 →“投喂后把小点心移到回收站”", 7000);
  }
  bool validDrop(const std::wstring &path) const {
    auto dot = path.rfind(L'.');
    if (dot == std::wstring::npos)
      return false;
    auto ext = path.substr(dot);
    std::wstring low = ext;
    for (auto &c : low)
      c = wchar_t(towlower(c));
    if (low == L".json" && path.size() > 7 && path.compare(path.size() - 12, 12, L".model3.json") == 0)
      return true;
    static const wchar_t *foods[] = {L".txt", L".png", L".cpp", L".json"};
    for (auto f : foods)
      if (low == f)
        return true;
    return false;
  }
  void feed(const std::wstring &p) {
    if (inFeed)
      return;
    inFeed = true;
    auto f = inspectFood(p);
    if (!f) {
      s.food(FoodResult::no);
      inFeed = false;
      frame();
      reschedule(33);
      return;
    }
    if (feedVerdict(f->size, limits) == FeedVerdict::tooBig) {
      // The size attribute is all we looked at; the file is never opened.
      s.food(FoodResult::heavy);
      inFeed = false;
      frame();
      reschedule(33);
      return;
    }
    auto r = life.feed(f->hash, f->type, wallTime());
    s.food(r);
    if ((r == FoodResult::fresh || r == FoodResult::again) && limits.recycle) {
      consent();
      if (recycleConsent) {
        auto stamped = files.recycle(f->path);
        if (stamped.empty())
          s.talk(wide(limits.failedLine), 3200); // metadata-only fallback, the file stays put
        else
          files.note(Note::models, "{\"kind\":\"recycled\",\"name\":\"" + stamped + "\"}");
      }
    }
    inFeed = false;
    frame();
    reschedule(33);
  }
  void payload(const std::vector<std::wstring> &list, V at) {
    if (paused() || !view)
      return;
    if (list.empty()) {
      s.food(FoodResult::no);
      s.talk(L"这个看不懂呢…", 2600);
      frame();
      return;
    }
    const auto &first = list.front();
    bool model = first.size() > 12 && first.compare(first.size() - 12, 12, L".model3.json") == 0;
    if (model) {
      importModel(first);
      if (!quickNotice.empty()) {
        s.talk(quickNotice, 4000);
        quickNotice.clear();
      }
      frame();
      return;
    }
    if (list.size() > 1) {
      s.food(FoodResult::no);
      s.talk(L"一次只能吃一份哦", 2600);
      frame();
      return;
    }
    if (!validDrop(first)) {
      s.food(FoodResult::no);
      s.talk(L"这个看不懂呢…", 2600);
      frame();
      return;
    }
    (void)at;
    feed(first);
  }
  void textDrop(std::wstring) {
    // Unknown text/URL payload: a single confused shake, nothing is parsed or stored.
    s.food(FoodResult::no);
    s.talk(L"这个看不懂呢…", 2600);
    frame();
  }
  void installDrop() {
    if (dropTarget || !hwnd)
      return;
    DropHooks h;
    h.cursor = [] { auto c = cursor(); return POINT{LONG(std::lround(c.x)), LONG(std::lround(c.y))}; };
    h.overActor = [this](POINT p) { return view && onHit({double(p.x), double(p.y)}); };
    h.validFile = [this](const std::wstring &path) { return validDrop(path); };
    h.hover = [this](bool valid, POINT) {
      if (view)
        view->dragHint(true, valid, cursor());
      frame();
    };
    h.leave = [this] {
      if (view)
        view->dragHint(false, false, {});
      frame();
    };
    h.files = [this](std::vector<std::wstring> list, POINT p) { payload(list, {double(p.x), double(p.y)}); };
    h.text = [this](std::wstring t, POINT) { textDrop(std::move(t)); };
    dropTarget = makeDropTarget(std::move(h));
    if (dropTarget)
      RegisterDragDrop(hwnd, dropTarget);
  }
  bool onHit(V p) const {
    return ellipse(p - s.walk.pos, s.scale, life.s.satiety == 0);
  }
  void toggle() {
    s.mute = !s.mute;
    if (s.mute) {
      audio.stop();
      s.audio.clear();
    }
    s.talk(s.mute ? L"我轻轻的。〔静音〕" : L"现在听得到啦。", 2000);
    frame();
  }
  void menu(bool recent) {
    if (modal || paused())
      return;
    HMENU m = CreatePopupMenu();
    if (!m)
      return;
    auto add = [&](int id, const wchar_t *t, UINT flags = 0) {
      AppendMenuW(m, MF_STRING | flags, UINT_PTR(id), t);
    };
    auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    auto types = life.lastFive();
    if (recent) {
      add(0, L"记忆里的味道 · 不增加饱食度", MF_DISABLED);
      sep();
      if (types.empty())
        add(0, L"还没有投喂记忆", MF_DISABLED);
      for (size_t i = 0; i < types.size(); ++i)
        add(100 + int(i), (L"尝一口  ." + wide(types[i])).c_str());
    } else {
      add(0, (L"桌边  /  " + std::to_wstring(life.s.satiety) + L" 饱食度").c_str(), MF_DISABLED);
      sep();
      add(1, L"投喂…");
      add(11, L"记忆投喂  Ctrl+Shift+F");
      sep();
      add(12, L"打开陪伴工作台");
      add(13, L"画给我看");
      add(14, L"我的回忆");
      if (s.intents.recognize || s.intents.compose)
        add(15, L"中断智能 · 回到纯规则");
      sep();
      add(2, L"陪我工作", s.mode == Mode::work ? MF_CHECKED : 0);
      add(3, L"聊聊天吧", s.mode == Mode::chat ? MF_CHECKED : 0);
      add(4, L"自由散步", s.mode == Mode::free ? MF_CHECKED : 0);
      sep();
      sep();
      if (!models.empty()) {
        HMENU sub = CreatePopupMenu();
        for (size_t i = 0; i < models.size() && i < 24; ++i)
          AppendMenuW(sub, MF_STRING | (models[i].id == modelId ? MF_CHECKED : 0), UINT_PTR(200 + i),
                      wide(models[i].name).c_str());
        AppendMenuW(sub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(sub, MF_STRING, 299, L"导入模型…");
        AppendMenuW(m, MF_POPUP, UINT_PTR(sub), L"模型");
      }
      add(9, L"投喂后把小点心移到回收站", recycleConsent ? MF_CHECKED : 0);
      add(5, L"静音  Ctrl+Shift+M", s.mute ? MF_CHECKED : 0);
      add(6, L"回到桌边  Esc");
      if (s.opt.luogu)
        add(7, L"查看洛谷配对码");
      sep();
      add(8, L"休息一下 · 退出");
    }
    modal = true;
    s.cancel();
    SetForegroundWindow(hwnd);
    V v = cursor();
    int id = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, int(v.x), int(v.y), hwnd,
                              nullptr);
    DestroyMenu(m);
    modal = false;
    if (id == 1) {
      modal = true;
      auto p = pick(hwnd);
      modal = false;
      if (!p.empty())
        feed(p);
    } else if (id >= 100 && id < 100 + int(types.size())) {
      s.food(FoodResult::again, false);
      s.talk(L"记忆里的 ." + wide(types[id - 100]) + L"，尝一小口。", 3000);
    } else if (id >= 12 && id <= 14 && bench)
      bench->show(id == 12 ? Page::home : id == 13 ? Page::doodle : Page::memories);
    else if (id >= 200 && id < 200 + int(models.size())) {
      switchModel(models[id - 200].id);
    } else if (id == 299) {
      modal = true;
      auto picked = pickModel(hwnd);
      modal = false;
      if (!picked.empty())
        importModel(picked);
      if (!quickNotice.empty()) {
        s.talk(quickNotice, 4000);
        quickNotice.clear();
      }
    } else if (id >= 200 && id < 200 + int(models.size())) {
      switchModel(models[id - 200].id);
    } else if (id == 299) {
      modal = true;
      auto picked = pickModel(hwnd);
      modal = false;
      if (!picked.empty())
        importModel(picked);
      if (!quickNotice.empty()) {
        s.talk(quickNotice, 4000);
        quickNotice.clear();
      }
    } else if (id == 15)
      pure();
    else if (id == 9) {
      recycleConsent = !recycleConsent;
      askedRecycle = true;
      files.note(Note::models, std::string("{\"kind\":\"consent\",\"recycle\":") +
                                   (recycleConsent ? "true}" : "false}"));
      s.talk(recycleConsent ? L"好呀，我会把吃过的那一份轻轻放进回收站。" : L"那我只记住名字和大小。", 3200);
    } else if (id == 11)
      menu(true);
    else if (id == 2)
      s.setMode(s.mode == Mode::work ? Mode::free : Mode::work);
    else if (id == 3)
      s.setMode(s.mode == Mode::chat ? Mode::free : Mode::chat);
    else if (id == 4)
      s.setMode(Mode::free);
    else if (id == 5)
      toggle();
    else if (id == 6)
      pure();
    else if (id == 7)
      s.talk(L"配对码：" + bridge.code(), 30000);
    else if (id == 8) {
      DestroyWindow(hwnd);
      return;
    }
    prev = GetTickCount64();
    if (IsWindow(hwnd)) {
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
      frame();
    }
  }
  void closed() {
    KillTimer(hwnd, 1);
    if (!paused())
      life.save(wallTime());
    if (power)
      UnregisterPowerSettingNotification(power);
    WTSUnRegisterSessionNotification(hwnd);
    for (int i = 1; i <= 2; ++i)
      UnregisterHotKey(hwnd, i);
    s.opt.input = s.opt.luogu = false;
    sensors();
    if (dropTarget) {
      RevokeDragDrop(hwnd);
      dropTarget->Release();
      dropTarget = nullptr;
    }
    bench.reset();
    view.reset();
  }
  LRESULT msg(UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_NCHITTEST: {
      V pt{double(GET_X_LPARAM(lp)), double(GET_Y_LPARAM(lp))};
      return view && (onHit(pt) || view->inBubble(pt, s)) ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_TIMER:
      if (wp == 1)
        onTimer();
      return 0;
    case WM_LBUTTONDOWN: {
      auto p = cursor();
      bubbleClick = view && view->inBubble(p, s);
      if (!right) {
        SetCapture(hwnd);
        if (!bubbleClick)
          s.press(p);
        reschedule(25);
        frame();
      }
      return 0;
    }
    case WM_MOUSEMOVE: {
      auto p = cursor();
      if (!right)
        s.move(p);
      else if (s.drag) {
        s.walk.warp(p - grab, s.bounds);
        frame();
      }
      return 0;
    }
    case WM_LBUTTONUP:
      if (!right) {
        if (bubbleClick && view && view->inBubble(cursor(), s))
          s.respond();
        else
          s.release();
        bubbleClick = false;
        ReleaseCapture();
        frame();
      }
      return 0;
    case WM_RBUTTONDOWN:
      right = true;
      s.cancel();
      down = cursor();
      grab = down - s.walk.pos;
      rightAt = s.now;
      SetCapture(hwnd);
      reschedule(25);
      return 0;
    case WM_RBUTTONUP:
      if (right) {
        right = false;
        bool held = s.drag;
        s.dragEnd();
        ReleaseCapture();
        if (!held) {
          if (life.s.satiety == 0)
            menuAt = s.now + 200;
          else
            menu(false);
        }
        frame();
      }
      return 0;
    case WM_CAPTURECHANGED:
      if (right || s.drag) {
        s.dragEnd();
        right = false;
      }
      return 0;
    case WM_MOUSEWHEEL:
      if (!s.drag) {
        s.wheel(GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA);
        reschedule(33);
      }
      return 0;
    case WM_HOTKEY:
      if (paused())
        return 0;
      if (wp == 4 && voiceHot && !memo.recording()) {
        voiceHold = true;
        voiceAt = GetTickCount64();
      }
      if (wp == 1)
        toggle();
      if (wp == 2)
        menu(true);
#ifdef PET_DEV
      if (wp == 3) {
        debug = !debug;
        frame();
      }
#endif
      return 0;
    case WM_INPUT:
      rawEvent(reinterpret_cast<HRAWINPUT>(lp));
      break;
    case WM_WTSSESSION_CHANGE:
      if (wp == WTS_SESSION_LOCK || wp == WTS_SESSION_UNLOCK) {
        locked = wp == WTS_SESSION_LOCK;
        pauseChanged();
      }
      return 0;
    case WM_POWERBROADCAST:
      if (wp == PBT_APMSUSPEND) {
        suspended = true;
        pauseChanged();
      } else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
        suspended = false;
        pauseChanged();
      } else if (wp == PBT_POWERSETTINGCHANGE && lp) {
        auto e = reinterpret_cast<POWERBROADCAST_SETTING *>(lp);
        if (IsEqualGUID(e->PowerSetting, GUID_CONSOLE_DISPLAY_STATE) && e->DataLength == sizeof(DWORD)) {
          DWORD value;
          memcpy(&value, e->Data, sizeof value);
          off = value == 0;
          pauseChanged();
        }
      }
      return TRUE;
    case WM_SETTINGCHANGE:
    case WM_DPICHANGED:
      if (view) {
        view->metrics();
        frame();
      }
      return 0;
    case WM_ENDSESSION:
      // ENDSESSION_CLOSEAPP is what an update/installer restart sends; a plain logoff is not a reboot.
      if (wp && (lp & ENDSESSION_CLOSEAPP)) {
        life.s.returnAfterShutdown = true;
        life.save(wallTime());
      }
      return 0;
    case WM_QUERYENDSESSION:
      if (lp & ENDSESSION_CLOSEAPP) {
        s.farewell();
        frame();
      }
      if (!paused())
        life.save(wallTime());
      return TRUE;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      closed();
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
  }
};
static LRESULT CALLBACK route(HWND w, UINT m, WPARAM wp, LPARAM lp) {
  auto *a = reinterpret_cast<App *>(GetWindowLongPtrW(w, GWLP_USERDATA));
  if (m == WM_NCCREATE) {
    a = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
    a->hwnd = w;
    SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(a));
  }
  if (!a)
    return DefWindowProcW(w, m, wp, lp);
  try {
    return a->msg(m, wp, lp);
  } catch (...) {
    a->life.dream = true;
    return 0;
  }
}
} // namespace pet
int WINAPI wWinMain(HINSTANCE module, HINSTANCE, PWSTR, int) {
  using namespace pet;
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  Handle single(CreateMutexW(nullptr, FALSE, L"Local\\DeskCompanion.Native.2.Singleton"));
  if (!single || GetLastError() == ERROR_ALREADY_EXISTS)
    return 0;
  if (FAILED(OleInitialize(nullptr)))
    return 1;
  int status = 0;
  try {
    App app;
    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = route;
    wc.hInstance = module;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    if (!RegisterClassExW(&wc))
      return 1;
    auto w = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_ACCEPTFILES, className,
        L"桌边 · PetDesk", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, module, &app);
    if (w) {
      app.init(w);
      MSG msg{};
      while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (app.bench && app.bench->dialog(msg))
          continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    } else
      status = 1;
  } catch (...) {
    status = 1;
  }
  OleUninitialize();
  return status;
}

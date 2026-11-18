#include "workbench.hpp"
#include "json.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <sstream>
namespace pet {
namespace {
constexpr COLORREF bg = RGB(248, 246, 244), panel = RGB(255, 253, 250), ink = RGB(50, 43, 56),
                   muted = RGB(110, 99, 113), plum = RGB(104, 70, 112), rose = RGB(239, 220, 220),
                   edge = RGB(217, 207, 210);
struct Brush {
  HBRUSH v;
  explicit Brush(COLORREF c) : v(CreateSolidBrush(c)) {}
  ~Brush() {
    DeleteObject(v);
  }
};
// These badges are self-contained SVG polygons; render just the tiny polygon subset, never open a web view.
constexpr const char *badgeSvg[] = {
    "<svg viewBox='0 0 64 64'><polygon points='32,3 51,12 57,31 47,50 32,60 17,50 7,31 13,12'/></svg>",
    "<svg viewBox='0 0 64 64'><polygon points='34,3 29,13 27,24 31,35 39,41 51,42 60,39 53,51 42,58 26,58 "
    "12,49 5,36 6,21 15,10'/></svg>",
    "<svg viewBox='0 0 64 64'><polygon points='32,57 6,32 4,20 10,10 20,7 32,15 44,7 54,10 60,20 "
    "58,32'/></svg>"};
const wchar_t *badgeTitles[] = {L"七日餐桌", L"深夜守护者", L"抚摸达人"};
std::vector<POINT> badgePoints(int i, int x, int y, double size) {
  std::string s = badgeSvg[i];
  auto begin = s.find("points='") + 8;
  auto str = s.substr(begin, s.find('\'', begin) - begin);
  std::replace(str.begin(), str.end(), ',', ' ');
  std::istringstream in(str);
  int a, b;
  std::vector<POINT> v;
  while (in >> a >> b)
    v.push_back({LONG(x + a * size / 64), LONG(y + b * size / 64)});
  return v;
}
} // namespace
struct Workbench::Impl {
  Scene &s;
  Vault &files;
  View &preview;
  Apply apply;
  std::function<std::wstring()> env;
  std::function<bool()> voiceReady;
  HWND win = nullptr;
  HFONT font = nullptr, heading = nullptr, small = nullptr;
  int dpi = 96;
  Page page = Page::home;
  std::vector<HWND> children;
  std::array<HWND, 3> sliders{};
  Drawpad pad;
  std::vector<Drawpad> gallery;
  int current = -1, detail = 0, warning = -1;
  ms redUntil = 0;
  bool white = false, pen = false;
  POINT previous{};
  std::wstring notice;
  std::vector<uint32_t> portrait;
  Impl(Scene &a, Vault &v, View &vw, Apply save, std::function<std::wstring()> e, std::function<bool()> ready)
      : s(a), files(v), preview(vw), apply(save), env(e), voiceReady(ready) {
    for (auto &row : files.notes(Note::doodles, 21)) {
      try {
        auto j = nlohmann::json::parse(row);
        auto d = Drawpad::from64(j.at("bits").get<std::string>());
        if (d && gallery.size() < 20)
          gallery.push_back(*d);
      } catch (...) {
      }
    }
  }
  ~Impl() {
    if (win)
      DestroyWindow(win);
    for (auto f : {font, heading, small})
      if (f)
        DeleteObject(f);
  }
  int px(double n) const {
    return int(std::lround(n * dpi / 96.));
  }
  RECT box(int x, int y, int w, int h) const {
    return {px(x), px(y), px(x + w), px(y + h)};
  }
  void paintText(HDC dc, const std::wstring &str, RECT r, HFONT f, COLORREF col,
                 UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetTextColor(dc, col);
    SetBkMode(dc, TRANSPARENT);
    auto old = SelectObject(dc, f);
    DrawTextW(dc, str.c_str(), int(str.size()), &r, flags);
    SelectObject(dc, old);
  }
  void text(HDC dc, const std::wstring &str, int x, int y, int w, int h, HFONT f = nullptr, COLORREF c = ink,
            UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    paintText(dc, str, box(x, y, w, h), f ? f : font, c, fmt);
  }
  void card(HDC dc, int x, int y, int w, int h, COLORREF c) {
    Brush b(c);
    auto r = box(x, y, w, h);
    FillRect(dc, &r, b.v);
  }
  HWND button(int id, std::wstring title, int x, int y, int w, int h) {
    auto c = CreateWindowW(L"BUTTON", title.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, px(x),
                           px(y), px(w), px(h), win, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    children.push_back(c);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, FALSE);
    return c;
  }
  bool option(int i) const {
    auto &o = s.opt;
    switch (i) {
    case 0:
      return o.intent && s.intents.recognize;
    case 1:
      return o.feedback && s.intents.compose;
    case 2:
      return o.luogu;
    case 3:
      return o.input;
    case 4:
      return o.meter;
    case 5:
      return o.ime;
    case 6:
      return o.gamma;
    case 7:
      return o.weather;
    default:
      return o.voice;
    }
  }
  std::wstring optName(int i) const {
    static const wchar_t *names[] = {L"15　意图识别", L"16　泛用反馈", L"洛谷通过事件",
                                     L"输入频率计数", L"系统音量峰值", L"输入法模式",
                                     L"显示器色温",   L"本机天气源",   L"离线语音备忘"};
    return std::wstring(option(i) ? L"●  " : L"○  ") + names[i];
  }
  void rebuild() {
    for (auto c : children)
      DestroyWindow(c);
    children.clear();
    sliders = {};
    static const wchar_t *nav[] = {L"概览", L"表达实验室", L"感知与隐私", L"画给我看", L"我的回忆"};
    for (int i = 0; i < 5; i++)
      button(10 + i, nav[i], 26 + i * 147, 91, 138, 36);
    if (page == Page::home) {
      button(30, L"陪我工作", 247, 294, 130, 38);
      button(31, L"聊聊天吧", 388, 294, 130, 38);
      button(32, L"自由散步", 529, 294, 130, 38);
      button(33, L"回到桌面中央", 247, 352, 186, 38);
      button(34, L"收起工作台", 445, 352, 162, 38);
    } else if (page == Page::intents) {
      button(50, optName(0), 30, 158, 222, 38);
      button(51, optName(1), 265, 158, 222, 38);
      button(80, L"导入语义策略", 502, 158, 228, 38);
      for (int i = 0; i < 3; i++) {
        double value = i == 0 ? s.style.intensity : i == 1 ? s.style.gaze : s.style.tempo;
        HWND c = CreateWindowW(TRACKBAR_CLASSW, L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_HORZ,
                               px(32 + i * 237), px(255), px(215), px(32), win, (HMENU)(INT_PTR)(90 + i),
                               GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, TBM_SETRANGE, TRUE, MAKELPARAM(i == 2 ? 50 : 0, i == 2 ? 150 : 120));
        SendMessageW(c, TBM_SETPOS, TRUE, int(std::round(value * 100)));
        sliders[i] = c;
        children.push_back(c);
      }
      for (int i = 1; i <= 5; i++)
        button(100 + i, std::array{L"庆祝", L"安慰", L"专注", L"好奇", L"提醒休息"}[i - 1],
               30 + (i - 1) * 143, 345, 132, 36);
      button(108, L"立即回到纯规则　Esc", 30, 410, 252, 37);
    } else if (page == Page::senses) {
      for (int i = 2; i <= 8; i++)
        button(50 + i, optName(i), 30 + ((i - 2) % 3) * 238, 184 + ((i - 2) / 3) * 62, 220, 44);
    } else if (page == Page::doodle) {
      button(120, white ? L"画笔：白色" : L"画笔：黑色", 286, 210, 180, 36);
      button(121, L"新的一张", 482, 210, 180, 36);
      button(122, L"画好了，给你看", 286, 267, 376, 42);
      button(123, L"上一张", 286, 336, 180, 36);
      button(124, L"下一张", 482, 336, 180, 36);
    } else {
      for (int i = 0; i < 3; i++)
        button(130 + i, badgeTitles[i], 40 + i * 238, 293, 205, 36);
    }
    InvalidateRect(win, nullptr, TRUE);
  }
  void show(Page target) {
    page = target;
    notice.clear();
    if (!win) {
      INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_BAR_CLASSES};
      InitCommonControlsEx(&ic);
      WNDCLASSW c{};
      c.lpfnWndProc = route;
      c.hInstance = GetModuleHandleW(nullptr);
      c.lpszClassName = L"DeskCompanion.Workbench";
      c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      RegisterClassW(&c);
      dpi = GetDpiForSystem();
      RECT r = box(0, 0, 770, 514);
      AdjustWindowRectExForDpi(&r, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0, dpi);
      win = CreateWindowExW(WS_EX_CONTROLPARENT, c.lpszClassName, L"Haru · 陪伴工作台",
                            WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, CW_USEDEFAULT,
                            CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, c.hInstance,
                            this);
      if (!win)
        return;
      font = makeFont(16, FW_NORMAL);
      small = makeFont(14, FW_NORMAL);
      heading = makeFont(27, FW_SEMIBOLD);
    }
    rebuild();
    SetTimer(win, 1, 240, nullptr);
    ShowWindow(win, SW_SHOWNORMAL);
    SetForegroundWindow(win);
  }
  HFONT makeFont(int h, int weight) {
    return CreateFontW(-px(h), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
  }
  void say(std::wstring n) {
    notice = std::move(n);
    InvalidateRect(win, nullptr, FALSE);
  }
  bool achievement(int i) const {
    return i == 0 ? s.life.s.streak : i == 1 ? s.life.s.guardian : s.life.s.strokes >= 10;
  }
  void paint(HDC dc) {
    card(dc, 0, 0, 770, 514, bg);
    card(dc, 0, 0, 770, 3, plum);
    text(dc, L"D E S K S I D E     /     C O M P A N I O N     L A B", 26, 11, 520, 19, small, plum);
    text(dc, L"Haru 的陪伴工作台", 26, 33, 570, 44, heading);
    text(dc, s.trusted ? L"●　模型已校验" : L"○　纯规则保护", 580, 40, 160, 32, small, plum);
    card(dc, 25, 145, 720, 314, panel);
    if (page == Page::home) {
      int W = 174, H = 230;
      portrait.assign(size_t(W) * H, 0);
      auto p = s.pose();
      preview.sample(portrait.data(), W, H, {W * .5, 105}, 1.25, p);
      for (auto &col : portrait) {
        int a = 255 - int(col >> 24);
        int r = ((col >> 16) & 255) + a * 237 / 255, g = ((col >> 8) & 255) + a * 229 / 255,
            b = (col & 255) + a * 232 / 255;
        col = (r << 16) | (g << 8) | b;
      }
      BITMAPINFO bi{};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = W;
      bi.bmiHeader.biHeight = -H;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      StretchDIBits(dc, px(46), px(166), px(W), px(H), 0, 0, W, H, portrait.data(), &bi, DIB_RGB_COLORS,
                    SRCCOPY);
      text(dc, L"HARU   /   官方样例  4-r.7", 46, 399, 215, 25, small, plum);
      text(dc, L"今天，一起做点什么？", 247, 169, 470, 43, heading);
      text(dc,
           L"饱食度  " + std::to_wstring(s.life.s.satiety) + L" / 100      今日投喂  " +
               std::to_wstring(s.life.today(s.wall)) + L" / 50",
           248, 228, 470, 30);
      text(dc,
           s.mode == Mode::work   ? L"正在陪你工作"
           : s.mode == Mode::chat ? L"正在聊天"
                                  : L"自由活动中",
           248, 259, 440, 25, small, muted);
    } else if (page == Page::intents) {
      text(dc, L"安全表达范围　/　越界会回弹，所有参数仍受模型 Min / Max 限制", 32, 206, 700, 26, small,
           muted);
      for (int i = 0; i < 3; i++) {
        bool red = warning == i && GetTickCount64() < ULONGLONG(redUntil);
        std::wstring v = i == 0 ? L"动作幅度　" : i == 1 ? L"视线灵敏度　" : L"节奏速度　";
        v += std::to_wstring(int(std::round((i == 0   ? s.style.intensity
                                             : i == 1 ? s.style.gaze
                                                      : s.style.tempo) *
                                            100))) +
             L"%";
        text(dc, v, 34 + i * 237, 229, 216, 27, font, red ? RGB(185, 53, 56) : ink);
      }
      text(dc, L"试演意图　/　相同标签冷却，不能打断的动作会先完成", 32, 307, 710, 30, small, muted);
      text(dc,
           L"当前：" + wide(label(s.intents.tag).name) + L"   ·   可用语义 " + std::to_wstring(s.capability),
           310, 412, 428, 36, small, plum);
    } else if (page == Page::senses) {
      text(dc, L"默认关闭。点选后会先说明调用的 Windows 函数；各项可单独撤回。", 30, 149, 710, 30, small,
           muted);
      text(dc, env(), 32, 403, 693, 36, small, plum);
    } else if (page == Page::doodle) {
      text(dc, L"64 × 64　/　黑与白", 48, 153, 220, 34, small, plum);
      std::array<uint32_t, 4096> bits{};
      for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
          bits[y * 64 + x] = pad.black(x, y) ? 0 : 0xffffff;
      BITMAPINFO bi{};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = 64;
      bi.bmiHeader.biHeight = -64;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      StretchDIBits(dc, px(48), px(193), px(192), px(192), 0, 0, 64, 64, bits.data(), &bi, DIB_RGB_COLORS,
                    SRCCOPY);
      auto rc = box(47, 192, 194, 194);
      Brush b(edge);
      FrameRect(dc, &rc, b.v);
      text(dc, L"画册　" + std::to_wstring(gallery.size()) + L" / 20", 286, 161, 385, 38, heading);
      text(dc, L"只收进本地画册，不生成图片文件。满 20 张后仍可以画给她看。", 286, 381, 400, 67, small, muted,
           DT_LEFT | DT_WORDBREAK);
    } else {
      for (int i = 0; i < 3; i++) {
        auto pts = badgePoints(i, px(104 + i * 238), px(188), px(72));
        Brush b(achievement(i) ? plum : edge);
        auto old = SelectObject(dc, b.v);
        auto penOld = SelectObject(dc, GetStockObject(NULL_PEN));
        Polygon(dc, pts.data(), int(pts.size()));
        SelectObject(dc, penOld);
        SelectObject(dc, old);
        text(dc, achievement(i) ? L"已点亮" : L"未点亮", 113 + i * 238, 261, 115, 24, small, muted);
      }
      const wchar_t *details[] = {L"七个连续的本地日期都有一次真实投喂。虚拟食物不计数。",
                                  L"23:00–06:00 在编辑器或已配对的洛谷前台，陪伴过一次。",
                                  L"完成 10 次温柔抚摸。当前："};
      text(dc,
           std::wstring(details[detail]) + (detail == 2 ? std::to_wstring(s.life.s.strokes) + L" 次" : L""),
           49, 365, 670, 57, font, ink, DT_LEFT | DT_WORDBREAK);
    }
    text(dc, notice.empty() ? L"LOCAL ONLY    ·    你的文件不是食物本身，只是一次邀请。" : notice, 28, 467,
         718, 39, small, notice.empty() ? muted : plum);
  }
  void alter(int i) {
    auto o = s.opt;
    bool *bits[] = {&o.intent, &o.feedback, &o.luogu,   &o.input, &o.meter,
                    &o.ime,    &o.gamma,    &o.weather, &o.voice};
    bool on = !option(i);
    if (on && i <= 1 && !s.trusted) {
      say(L"模型校验未通过，仍由纯规则陪伴。");
      return;
    }
    if (on && i == 8 && !voiceReady()) {
      say(L"离线 Whisper-tiny 未就绪；没有打开麦克风。");
      return;
    }
    static const wchar_t *sources[] = {
        L"本地 ONNX 事件查找表。输入：已获准的投喂、模式、电量及互动事件。不会自动启用下面任何传感器。",
        L"读取本机模型：csmGetParameterIds / MinimumValues / MaximumValues "
        L"与物理绑定。只做查表插值与安全钳位。",
        L"WSAStartup / bind(127.0.0.1:38417) / recv。仅接收配对扩展发来的“在场”和题号，不接收 URL 或历史。",
        L"RegisterRawInputDevices / "
        L"GetRawInputData。仅读鼠标按下标志与键盘按下/释放标志，存分桶次数，不读键值、文本或点击目标。",
        L"IMMDeviceEnumerator::GetDefaultAudioEndpoint / IAudioSessionManager2::GetSessionEnumerator / "
        L"IAudioMeterInformation::GetPeakValue / IAudioEndpointVolume::GetMute。仅峰值与静音，不录制。",
        L"GetForegroundWindow / GetWindowThreadProcessId / GetKeyboardLayout / ImmGetDefaultIMEWnd / "
        L"SendMessageTimeoutW(IMC_GETOPENSTATUS、IMC_GETCONVERSIONMODE)。不读文字或候选。",
        L"GetDeviceGammaRamp。仅读三条 256 项显示 LUT，估算冷暖；不截屏、不改显示设置。",
        L"后台仅读程序私有 config/weather.json 的城市与 WMO 天气码；本地缓存 24 "
        L"小时。不使用定位，也不向气象服务上传城市。",
        L"RegisterHotKey / waveInOpen / waveInAddBuffer。仅按住 Ctrl+Shift+V 时录音，上限 10 秒；Whisper "
        L"在本机处理内存 PCM，仅追加文本 SHA-256。"};
    if (on) {
      std::wstring q = sources[i];
      if (i <= 1)
        q += L"\n基础桌面数据源：GetFileAttributesExW（投喂元数据）、GetForegroundWindow / GetWindowRect / "
             L"QueryFullProcessImageNameW（前台应用类别）、GetSystemPowerStatus（电源）、GetTickCount64（作息"
             L"）。";
      q += L"\n\n允许这一项吗？取消不会启用。";
      if (MessageBoxW(win, q.c_str(), L"启用前 · 精确数据源",
                      MB_OKCANCEL | MB_ICONINFORMATION | MB_DEFBUTTON2) != IDOK)
        return;
    }
    *bits[i] = on;
    if (apply(o))
      say(on ? L"这项已单独启用。" : L"已关闭这项感知。");
    else
      say(L"好像做了个奇怪的梦呢，未能保存选择。");
    rebuild();
  }
  void command(int id) {
    if (id >= 10 && id < 15) {
      page = Page(id - 10);
      notice.clear();
      rebuild();
      return;
    }
    if (id >= 50 && id <= 58) {
      alter(id - 50);
      return;
    }
    if (id >= 101 && id <= 105) {
      if (!s.intents.recognize && !s.intents.compose) {
        say(L"先打开 15 或 16，再试演。");
        return;
      }
      bool play = s.intents.request(Tag(id - 100), s.now);
      say(play ? L"请看桌面上的 Haru。" : L"同一标签冷却中，或已按优先级等候。");
      return;
    }
    if (id >= 130 && id <= 132) {
      detail = id - 130;
      notice.clear();
      InvalidateRect(win, nullptr, FALSE);
      return;
    }
    switch (id) {
    case 30:
      s.setMode(Mode::work);
      break;
    case 31:
      s.setMode(Mode::chat);
      break;
    case 32:
      s.setMode(Mode::free);
      break;
    case 33:
      s.reset();
      break;
    case 34:
      KillTimer(win, 1);
      ShowWindow(win, SW_HIDE);
      return;
    case 80: {
      auto raw = files.setting(L"policy.json");
      auto imported = policy(raw);
      if (!imported) {
        say(L"策略不完整，保留现有表达。");
        return;
      }
      s.style = *imported;
      auto j = nlohmann::json::parse(raw);
      std::wstring missing;
      for (auto &item : j["semantics"]) {
        auto name = item.get<std::string>();
        if (!preview.supports(name))
          missing += L" " + wide(name);
      }
      files.note(Note::style, raw.find('\n') == raw.npos ? raw : j.dump());
      say(missing.empty() ? L"策略的语义通道均已匹配。" : L"已匹配可用通道；跳过：" + missing);
      rebuild();
      return;
    }
    case 108: {
      auto o = s.opt;
      o.intent = o.feedback = false;
      apply(o);
      s.reset();
      say(L"已中断所有意图与残留效果。");
      rebuild();
      return;
    }
    case 120:
      white = !white;
      rebuild();
      return;
    case 121:
      pad = {};
      current = -1;
      say(L"新画布已就绪，原有收藏未改变。");
      return;
    case 122: {
      if (std::none_of(pad.bits.begin(), pad.bits.end(), [](auto v) { return v != 0; })) {
        say(L"先画一笔给我看看吧。");
        return;
      }
      if (gallery.size() < 20) {
        if (!files.note(Note::doodles, nlohmann::json{{"bits", pad.to64()}}.dump())) {
          say(L"好像做了个奇怪的梦呢，没有写入画册。");
          return;
        }
        gallery.push_back(pad);
        current = int(gallery.size()) - 1;
        say(L"已收藏，Haru 正在看你的画。");
      } else
        say(L"画册满了，这张只在眼前看看，不替换旧画。");
      s.observeDoodle(int(s.rng() % 10));
      return;
    }
    case 123:
    case 124:
      if (!gallery.empty()) {
        current = (current + (id == 124 ? 1 : -1) + int(gallery.size())) % int(gallery.size());
        if (current < 0)
          current = 0;
        pad = gallery[current];
        say(L"第 " + std::to_wstring(current + 1) + L" 张。修改画布不会覆盖它。");
      }
      return;
    }
    InvalidateRect(win, nullptr, FALSE);
  }
  void drawButton(DRAWITEMSTRUCT *d) {
    wchar_t t[90]{};
    GetWindowTextW(d->hwndItem, t, 90);
    bool selected =
        (d->CtlID == 10 + unsigned(page)) || (d->CtlID >= 50 && d->CtlID <= 58 && option(d->CtlID - 50));
    Brush b(selected ? plum : d->itemState & ODS_SELECTED ? rose : RGB(242, 237, 238));
    FillRect(d->hDC, &d->rcItem, b.v);
    paintText(d->hDC, t, d->rcItem, font, selected ? RGB(255, 253, 252) : ink,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (d->itemState & ODS_FOCUS) {
      auto r = d->rcItem;
      InflateRect(&r, -3, -3);
      DrawFocusRect(d->hDC, &r);
    }
  }
  static LRESULT CALLBACK route(HWND w, UINT m, WPARAM a, LPARAM b) {
    Impl *p = (Impl *)GetWindowLongPtrW(w, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
      p = (Impl *)((CREATESTRUCTW *)b)->lpCreateParams;
      SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)p);
      p->win = w;
    }
    if (!p)
      return DefWindowProcW(w, m, a, b);
    try {
      return p->msg(m, a, b);
    } catch (...) {
      p->say(L"好像做了个奇怪的梦呢");
      return 0;
    }
  }
  LRESULT msg(UINT m, WPARAM a, LPARAM b) {
    switch (m) {
    case WM_COMMAND:
      command(LOWORD(a));
      return 0;
    case WM_CLOSE:
      KillTimer(win, 1);
      ShowWindow(win, SW_HIDE);
      return 0;
    case WM_DRAWITEM:
      drawButton((DRAWITEMSTRUCT *)b);
      return TRUE;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      auto dc = BeginPaint(win, &ps);
      paint(dc);
      EndPaint(win, &ps);
      return 0;
    }
    case WM_TIMER:
      InvalidateRect(win, nullptr, FALSE);
      return 0;
    case WM_HSCROLL: {
      int i = GetDlgCtrlID((HWND)b) - 90;
      if (i < 0 || i > 2)
        return 0;
      bool clip;
      double v = safeSlider(i, SendMessageW((HWND)b, TBM_GETPOS, 0, 0) / 100., clip);
      if (i == 0)
        s.style.intensity = v;
      else if (i == 1)
        s.style.gaze = v;
      else
        s.style.tempo = v;
      if (clip) {
        SendMessageW((HWND)b, TBM_SETPOS, TRUE, int(std::round(v * 100)));
        warning = i;
        redUntil = GetTickCount64() + 1200;
        say(L"已回到安全范围，不会让模型越界。");
      }
      if (LOWORD(a) == TB_ENDTRACK) {
        files.note(Note::style, nlohmann::json{{"version", 1},
                                               {"semantics", {"breath", "head_tilt"}},
                                               {"intensity", s.style.intensity},
                                               {"gaze", s.style.gaze},
                                               {"tempo", s.style.tempo}}
                                    .dump());
      }
      InvalidateRect(win, nullptr, FALSE);
      return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP: {
      if (page != Page::doodle)
        break;
      POINT point{GET_X_LPARAM(b), GET_Y_LPARAM(b)};
      auto r = box(48, 193, 192, 192);
      if (m == WM_LBUTTONDOWN) {
        if (!PtInRect(&r, point))
          break;
        pen = true;
        SetCapture(win);
      }
      if (!pen)
        break;
      POINT pt{std::clamp((point.x - r.left) * 64 / (r.right - r.left), 0L, 63L),
               std::clamp((point.y - r.top) * 64 / (r.bottom - r.top), 0L, 63L)};
      if (m == WM_LBUTTONDOWN)
        previous = pt;
      pad.line(previous.x, previous.y, pt.x, pt.y, !white);
      previous = pt;
      InvalidateRect(win, &r, FALSE);
      if (m == WM_LBUTTONUP) {
        pen = false;
        ReleaseCapture();
      }
      return 0;
    }
    case WM_CAPTURECHANGED:
      pen = false;
      break;
    }
    return DefWindowProcW(win, m, a, b);
  }
};
Workbench::Workbench(Scene &s, Vault &v, View &w, Apply a, std::function<std::wstring()> e,
                     std::function<bool()> ready)
    : p(std::make_unique<Impl>(s, v, w, a, e, ready)) {}
Workbench::~Workbench() = default;
void Workbench::show(Page page) {
  p->show(page);
}
void Workbench::close() {
  if (p->win)
    SendMessageW(p->win, WM_CLOSE, 0, 0);
}
bool Workbench::dialog(MSG &m) {
  return p->win && IsWindowVisible(p->win) && IsDialogMessageW(p->win, &m);
}
} // namespace pet

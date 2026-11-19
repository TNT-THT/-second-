#include "view.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <winreg.h>
#include "layout.hpp"
#include <algorithm>
using namespace Gdiplus;
namespace pet {
View::View(HWND win, Vault &v) : hwnd(win), fx(v), files(v) {
  GdiplusStartupInput init;
  GdiplusStartup(&gdip, &init, nullptr);
  dc = CreateCompatibleDC(nullptr);
  metrics();
}
void View::allocate() {
  if (dib) {
    SelectObject(dc, old);
    DeleteObject(dib);
  }
  BITMAPINFO b{};
  b.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  b.bmiHeader.biWidth = w;
  b.bmiHeader.biHeight = -h;
  b.bmiHeader.biPlanes = 1;
  b.bmiHeader.biBitCount = 32;
  b.bmiHeader.biCompression = BI_RGB;
  dib = CreateDIBSection(dc, &b, DIB_RGB_COLORS, reinterpret_cast<void **>(&bits), nullptr, 0);
  if (dib)
    old = SelectObject(dc, dib);
}
View::~View() {
  if (dib) {
    SelectObject(dc, old);
    DeleteObject(dib);
  }
  if (dc)
    DeleteDC(dc);
  if (gdip)
    GdiplusShutdown(gdip);
}
static bool lightTheme() {
  DWORD light = 1, cb = sizeof light;
  // Documented read-only preference; no write path exists in this app.
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &cb) != ERROR_SUCCESS)
    return true;
  return light != 0;
}
void View::metrics() {
  UINT dpi = GetDpiForWindow(hwnd);
  if (!dpi)
    dpi = 96;
  // Accessibility text scale is a documented read-only preference; no WinRT activation is needed.
  double textScale = 1;
  DWORD scale = 100, cb = sizeof scale;
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Accessibility", L"TextScaleFactor",
                   RRF_RT_REG_DWORD, nullptr, &scale, &cb) == ERROR_SUCCESS &&
      scale >= 100 && scale <= 225)
    textScale = scale / 100.;
  darkTheme = !lightTheme();
  NONCLIENTMETRICSW n{sizeof(n)};
  SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof n, &n, 0);
  font = std::max(16. * dpi / 96 * textScale, double(std::abs(n.lfMessageFont.lfHeight)));
  int nw = int(std::max(340., font * 17.5)), nh = int(310 + font * 3.3);
  if (nw != w || nh != h) {
    w = nw;
    h = nh;
    allocate();
  }
}
V View::center() const {
  return {w * .5, h - 138.};
}
bool View::inBubble(V p, const Scene &s) const {
  return s.mode == Mode::chat && !s.caption().empty() && bubble.r > bubble.l &&
         bubble.has(p - s.walk.pos + center());
}
static void rounded(Graphics &g, RectF r, REAL rad, Color fill, Color line) {
  GraphicsPath path;
  REAL k = rad * 2;
  path.AddArc(r.X, r.Y, k, k, 180, 90);
  path.AddArc(r.GetRight() - k, r.Y, k, k, 270, 90);
  path.AddArc(r.GetRight() - k, r.GetBottom() - k, k, k, 0, 90);
  path.AddArc(r.X, r.GetBottom() - k, k, k, 90, 90);
  path.CloseFigure();
  SolidBrush brush(fill);
  Pen pen(line, 1.f);
  g.FillPath(&brush, &path);
  g.DrawPath(&pen, &path);
}
static void text(Graphics &g, const std::wstring &s, RectF r, REAL px, Color c, bool bold = false) {
  Font f(L"Microsoft YaHei UI", px, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
  SolidBrush brush(c);
  StringFormat fmt;
  fmt.SetAlignment(StringAlignmentCenter);
  fmt.SetLineAlignment(StringAlignmentCenter);
  fmt.SetTrimming(StringTrimmingEllipsisCharacter);
  g.DrawString(s.c_str(), int(s.size()), &f, r, &fmt, &brush);
}
bool View::loadModel(Vault &v, const ModelEntry &entry) {
  // Reuse the live render pipeline: no second decoder, no duplicate texture budget.
  actor.actor.discard();
  if (!actor.load(v, entry))
    return false;
  preloaded = true;
  bubble = {0, 0, -1, -1};
  return true;
}
void View::show(Scene &s, double mouth
#ifdef PET_DEV
                ,
                bool debug, double fps, size_t kib
#endif
) {
  if (!bits)
    return;
  Pose p = s.pose();
  auto rt = GetTickCount64();
  if (!checked) {
    s.trusted = preloaded || actor.load(files, haruModelEntry());
    checked = true;
    integrityAt = rt + 300000;
    s.capability = actor.actor.channels();
    if (!s.trusted) {
      s.smartStop();
      s.talk(L"好像做了个奇怪的梦呢", 4500);
    }
  } else if (s.trusted && rt >= integrityAt) {
    integrityAt = rt + 300000;
    if (!actor.verify(files)) {
      s.trusted = false;
      s.capability = 0;
      s.smartStop();
      s.talk(L"好像做了个奇怪的梦呢", 4500);
    }
  }
  V waist = center();
  V paint = waist + V{(p.offset + p.rub) * s.scale, p.jump * s.scale};
  POINT origin{LONG(std::lround(s.walk.pos.x - waist.x)), LONG(std::lround(s.walk.pos.y - waist.y))};
  memset(bits, 0, size_t(w) * h * 4);
  actor.paint(bits, w, h, paint, s.scale, p, mouth);
  Bitmap surface(w, h, w * 4, PixelFormat32bppPARGB, reinterpret_cast<BYTE *>(bits));
  {
    Graphics g(&surface);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    REAL cx = REAL(waist.x), cy = REAL(waist.y);
    bool sleep = s.life.s.satiety == 0;
    REAL rx = REAL((sleep ? 20 : 30) * s.scale), ry = REAL((sleep ? 30 : 45) * s.scale);
    SolidBrush hit(Color(1, 25, 20, 35));
    g.FillEllipse(&hit, cx - rx, cy - ry, 2 * rx, 2 * ry);
    if (!actor.present()) {
      auto save = g.Save();
      g.TranslateTransform(REAL(paint.x), REAL(paint.y));
      g.RotateTransform(REAL(p.lean));
      RectF r(-rx, -ry, 2 * rx, 2 * ry);
      Color edge = sleep ? Color(238, 127, 130, 144) : Color(238, 122, 104, 176);
      LinearGradientBrush pearl(r, Color(248, 255, 251, 255),
                                sleep ? Color(245, 221, 221, 230) : Color(247, 223, 213, 247),
                                LinearGradientModeForwardDiagonal);
      g.FillEllipse(&pearl, r);
      Pen outline(edge, 1.25f);
      g.DrawEllipse(&outline, r);
      Pen orbit(Color(190, 255, 255, 255), 1.f);
      g.DrawEllipse(&orbit, -rx + 3, -ry + 3, 2 * rx - 6, 2 * ry - 6);
      SolidBrush ink(edge);
      REAL rate = REAL(.7 + .3 * p.breath);
      PointF diamond[] = {{0, -ry * .68f - 3 * rate},
                          {3 * rate, -ry * .68f},
                          {0, -ry * .68f + 3 * rate},
                          {-3 * rate, -ry * .68f}};
      g.FillPolygon(&ink, diamond, 4);
      text(g, sleep ? L"休息" : L"投喂", RectF(-rx, -ry * .45f, rx * 2, ry * .9f),
           REAL(std::max(12., 15 * s.scale)), Color(255, 73, 59, 96), true);
      text(g, std::to_wstring(s.life.s.satiety), RectF(-rx, ry * .23f, 2 * rx, ry * .45f), REAL(11 * s.scale),
           Color(255, 104, 90, 132));
      g.Restore(save);
      std::wstring label = L"模型待接入";
      RectF pill(cx - 52, cy + ry + 5, 104, 23);
      rounded(g, pill, 11, Color(236, 251, 249, 255), Color(191, 184, 170, 209));
      text(g, label, pill, 13, Color(255, 92, 74, 119));
    }
    auto f = s.effect.kind;
    int frames = f == Fx::flowers ? 12 : 8;
    int frame = 0;
    if (f != Fx::none && s.now < s.effect.at + s.effect.len)
      frame = int((s.now - s.effect.at) * frames / s.effect.len);
    else if (p.spring) {
      f = Fx::lantern;
      frame = int((s.now / 220) % 6);
    } else
      f = Fx::none;
    fx.request(f, f == Fx::none ? -1 : frame);
    std::array<uint32_t, 4096> pixels;
    if (f != Fx::none && fx.copy(f, pixels)) {
      Bitmap img(64, 64, 256, PixelFormat32bppPARGB, (BYTE *)pixels.data());
      g.DrawImage(&img, REAL(paint.x + (f == Fx::lantern ? 8 : -32)), REAL(paint.y - ry - 28), 64.f, 64.f);
    }
    auto words = s.caption();
    bubble = {0, 0, -1, -1};
    if (!words.empty()) {
      REAL px = REAL(std::max(16. * GetDpiForWindow(hwnd) / 96, font * (s.mode == Mode::work ? .8 : 1.)));
      REAL bw = REAL(std::max(84., words.size() * double(px) + 26));
      int rows = std::max(1, int(std::ceil(words.size() * double(px) / (double(bw) - 22))));
      REAL bh = REAL(rows * px * 1.5 + 10);
      // Placement is decided in virtual-screen coordinates against this monitor's work area, so a bubble
      // never straddles an edge, and the arrow keeps pointing at the pet.
      RECT area{};
      MONITORINFO mi{sizeof(mi)};
      if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        area = mi.rcWork;
      else
        area = {0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
      const int headY = int(std::lround(paint.y - (actor.present() ? 80. : 47.) * s.scale));
      auto placed = placeBubble({area.left, area.top, area.right, area.bottom},
                                origin.x + int(std::lround(paint.x)), origin.y + headY, int(bw), int(bh));
      RectF rr(REAL(placed.box.l - origin.x), REAL(placed.box.t - origin.y), REAL(placed.box.width()),
               REAL(placed.box.height()));
      rr.X = std::clamp(rr.X, 0.f, std::max(0.f, REAL(w) - rr.Width));
      rr.Y = std::clamp(rr.Y, 0.f, std::max(0.f, REAL(h) - rr.Height));
      rounded(g, rr, 12, Color(249, 255, 252, 251), Color(238, 199, 184, 211));
      // Tail from the box toward the pet, clamped so it always touches the face.
      const int tailLo = int(std::lround(rr.X)) + 14, tailHi = int(std::lround(rr.GetRight())) - 14;
      const int tailAt = std::clamp(int(placed.tipX - origin.x), tailLo, std::max(tailLo, tailHi));
      const REAL tailX = REAL(tailAt);
      const REAL tailY = placed.below ? rr.Y : rr.GetBottom();
      const REAL tipY = placed.below ? rr.Y - 7.f : rr.GetBottom() + 7.f;
      PointF tail[] = {{tailX - 6.f, tailY}, {tailX + 6.f, tailY}, {tailX, tipY}};
      SolidBrush tailFill(Color(249, 255, 252, 251));
      g.FillPolygon(&tailFill, tail, 3);
      text(g, words, rr, px, Color(255, 62, 45, 72));
      bubble = {rr.X, rr.Y, rr.GetRight(), rr.GetBottom()};
    }
    if (dragActive) {
      // Themed cue: accent ring for a valid payload, dim mask plus a forbidden mark otherwise.
      Color accent = darkTheme ? Color(255, 122, 178, 255) : Color(255, 60, 120, 216);
      Color danger = darkTheme ? Color(255, 255, 138, 138) : Color(255, 198, 74, 74);
      REAL bx = REAL(paint.x), by = REAL(paint.y) - REAL(38 * s.scale);
      REAL ex = REAL(52 * s.scale), ey = REAL(74 * s.scale);
      if (dragValid) {
        Pen ring(accent, 2.f);
        Pen halo(Color(80, accent.GetR(), accent.GetG(), accent.GetB()), 5.f);
        g.DrawEllipse(&halo, bx - ex, by - ey, 2 * ex, 2 * ey);
        g.DrawEllipse(&ring, bx - ex, by - ey, 2 * ex, 2 * ey);
      } else {
        SolidBrush mask(Color(96, 24, 22, 32));
        g.FillEllipse(&mask, bx - ex, by - ey, 2 * ex, 2 * ey);
        Pen slash(danger, 3.f);
        REAL r = 13 * REAL(s.scale);
        g.DrawEllipse(&slash, bx - r, by - r, 2 * r, 2 * r);
        g.DrawLine(&slash, bx - r * .7f, by + r * .7f, bx + r * .7f, by - r * .7f);
      }
    }
    if (s.now < s.scaleLabel) {
      RectF rr(cx + rx + 5, cy - 13, 63, 26);
      rounded(g, rr, 10, Color(240, 251, 251, 255), Color(220, 197, 190, 215));
      text(g, std::to_wstring(int(std::round(s.goalScale * 100))) + L"%", rr, 14, Color(255, 72, 62, 92));
    }
#ifdef PET_DEV
    if (debug) {
      Pen a(Color(255, 255, 40, 55)), b(Color(255, 34, 235, 67));
      g.DrawEllipse(&a, cx - rx, cy - ry, rx * 2, ry * 2);
      V path = s.walk.target - s.walk.pos;
      g.DrawLine(&b, cx, cy, cx + REAL(path.x), cy + REAL(path.y));
      RectF rr(2, REAL(h - 42), REAL(w - 4), 40);
      rounded(g, rr, 6, Color(245, 32, 35, 40), Color(245, 60, 65, 70));
      text(g,
           wide(mood(s.life.s.satiety)) + L" · " + std::to_wstring(s.life.s.satiety) + L" · " +
               std::to_wstring(int(fps)) + L" FPS · " + std::to_wstring(kib / 1024) + L" MiB",
           rr, 16, Color(255, 255, 255, 255));
    }
#endif
  }
  POINT at = origin, src{0, 0};
  SIZE size{w, h};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(hwnd, nullptr, &at, &size, dc, &src, 0, &blend, ULW_ALPHA);
}
} // namespace pet

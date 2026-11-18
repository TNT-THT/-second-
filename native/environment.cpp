#include "environment.hpp"
#include "json.hpp"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <imm.h>
#ifndef IMC_GETOPENSTATUS
#define IMC_GETOPENSTATUS 0x0005
#define IMC_GETCONVERSIONMODE 0x0001
#endif
// MinGW's header only forward-declares the published IAudioMeterInformation ABI.
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
struct IAudioMeterInformation : IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT, float *) = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *) = 0;
};
#endif
#include <ctime>
namespace pet {
namespace {
template <class T> struct Ptr {
  T *p = nullptr;
  ~Ptr() {
    reset();
  }
  void reset() {
    if (p) {
      p->Release();
      p = nullptr;
    }
  }
  T **put() {
    reset();
    return &p;
  }
  T *operator->() const {
    return p;
  }
};
// Render endpoints only. Never IAudioCaptureClient, loopback recording or audio samples.
struct PeakMeter {
  Ptr<IMMDeviceEnumerator> enumerator;
  Ptr<IMMDevice> speaker;
  Ptr<IAudioSessionManager2> sessions;
  Ptr<IAudioEndpointVolume> endpoint;
  Ptr<IAudioSessionEnumerator> list;
  ms renew = 0;
  bool sample(double &peak, bool &muted, ms now) {
    if (!enumerator.p && FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                                 IID_PPV_ARGS(enumerator.put()))))
      return false;
    if (now >= renew) {
      renew = now + 2500;
      list.reset();
      speaker.reset();
      sessions.reset();
      endpoint.reset();
      if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, speaker.put())))
        return false;
      speaker->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr,
                        (void **)endpoint.put());
      if (FAILED(speaker->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr,
                                   (void **)sessions.put())))
        return false;
      sessions->GetSessionEnumerator(list.put());
    }
    if (!list.p)
      return false;
    BOOL mute = FALSE;
    if (endpoint.p)
      endpoint->GetMute(&mute);
    muted = bool(mute);
    peak = 0;
    int n = 0;
    if (FAILED(list->GetCount(&n)))
      return false;
    for (int i = 0; i < std::min(n, 32); ++i) {
      Ptr<IAudioSessionControl> control;
      Ptr<IAudioSessionControl2> c2;
      Ptr<IAudioMeterInformation> meter;
      AudioSessionState state;
      if (FAILED(list->GetSession(i, control.put())) || FAILED(control->GetState(&state)) ||
          state != AudioSessionStateActive)
        continue;
      DWORD pid = 0;
      if (SUCCEEDED(control->QueryInterface(IID_PPV_ARGS(c2.put()))) && SUCCEEDED(c2->GetProcessId(&pid)) &&
          pid == GetCurrentProcessId())
        continue;
      if (SUCCEEDED(control->QueryInterface(
              GUID{0xc02216f6, 0x8c67, 0x4b5b, {0x9d, 0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64}},
              (void **)meter.put()))) {
        float f = 0;
        if (SUCCEEDED(meter->GetPeakValue(&f)))
          peak = std::max(peak, double(f));
      }
    }
    return true;
  }
};
// Only a mode bit. Unknown TSF/isolated windows stay unknown. Never request text, candidates or composition.
std::optional<bool> imeMode(HWND w) {
  if (!w)
    return {};
  auto lang = LOWORD(reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(GetWindowThreadProcessId(w, nullptr))));
  if (PRIMARYLANGID(lang) != LANG_CHINESE)
    return false;
  auto im = ImmGetDefaultIMEWnd(w);
  DWORD_PTR open = 0, mode = 0;
  if (!im || !SendMessageTimeoutW(im, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0, SMTO_ABORTIFHUNG, 20, &open))
    return {};
  if (!open)
    return false;
  if (!SendMessageTimeoutW(im, WM_IME_CONTROL, IMC_GETCONVERSIONMODE, 0, SMTO_ABORTIFHUNG, 20, &mode))
    return {};
  return (mode & IME_CMODE_NATIVE) != 0;
}
// The gamma LUT is not a colorimeter. Estimate correlated white point under an sRGB/D65 assumption.
std::optional<double> whitePoint(HWND w) {
  MONITORINFOEXW mi{};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi))
    return {};
  HDC dc = CreateDCW(L"DISPLAY", mi.szDevice, nullptr, nullptr);
  if (!dc)
    return {};
  WORD ramp[3][256]{};
  BOOL ok = GetDeviceGammaRamp(dc, ramp);
  DeleteDC(dc);
  if (!ok)
    return {};
  double rgb[3]{};
  for (int k = 0; k < 3; ++k) {
    for (int i = 160; i < 256; i += 8)
      rgb[k] += std::pow(ramp[k][i] / 65535., 2.2);
  }
  double x = .4124564 * rgb[0] + .3575761 * rgb[1] + .1804375 * rgb[2],
         y = .2126729 * rgb[0] + .7151522 * rgb[1] + .072175 * rgb[2],
         z = .0193339 * rgb[0] + .119192 * rgb[1] + .9503041 * rgb[2], sum = x + y + z;
  if (sum < .001)
    return {};
  double n = (x / sum - .3320) / (.1858 - y / sum),
         kelvin = 449 * n * n * n + 3525 * n * n + 6823.3 * n + 5520.33;
  return kelvin >= 1000 && kelvin <= 30000 ? std::optional<double>(kelvin) : std::nullopt;
}
} // namespace
Environment::~Environment() {
  quit = true;
  cv.notify_all();
  if (worker.joinable())
    worker.join();
}
void Environment::configure(const Options &o, bool pause) {
  doMeter = o.meter && !pause;
  doWeather = o.weather && !pause;
  if ((doMeter || doWeather) && !worker.joinable())
    worker = std::thread(&Environment::loop, this);
  cv.notify_all();
}
void Environment::loop() {
  HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  {
    std::unique_ptr<PeakMeter> meter;
    sec cacheAt = 0, tryAt = 0;
    bool cacheRead = false;
    for (;;) {
      if (quit)
        break;
      bool m = doMeter, w = doWeather;
      if (!m)
        meter.reset();
      Reading reading;
      {
        std::lock_guard<std::mutex> l(mu);
        reading = rr;
      }
      reading.meterOK = false;
      if (m && SUCCEEDED(com)) {
        if (!meter)
          meter = std::make_unique<PeakMeter>();
        reading.meterOK = meter->sample(reading.peak, reading.muted, GetTickCount64());
      }
      auto parse = [&](const std::string &line, bool cache) {
        try {
          auto j = nlohmann::json::parse(line);
          if (j.size() != (cache ? 3 : 2))
            return false;
          auto city = j.at("city").get<std::string>();
          int code = j.at("code").get<int>();
          if (city.empty() || city.size() > 80 || code < 0 || code > 99)
            return false;
          auto ts = cache ? epoch(j.at("ts").get<std::string>()) : std::optional<sec>(std::time(nullptr));
          if (!ts)
            return false;
          reading.city = city;
          reading.weather = code;
          reading.forecastAt = cacheAt = *ts;
          if (!cache) {
            j["ts"] = iso(*ts);
            files.note(Note::weather, j.dump());
          }
          return true;
        } catch (...) {
          return false;
        }
      };
      if (w) {
        if (!cacheRead) {
          cacheRead = true;
          for (auto &line : files.notes(Note::weather, 1))
            parse(line, true);
        }
        sec t = std::time(nullptr);
        if ((!cacheAt || t - cacheAt >= 86400) && t >= tryAt) {
          tryAt = t + 3600;
          parse(files.setting(L"weather.json", 1024), false);
        }
      } else {
        cacheRead = false;
        cacheAt = tryAt = 0;
        reading.city.clear();
        reading.weather = -1;
      }
      std::unique_lock<std::mutex> lock(mu);
      rr = std::move(reading);
      cv.wait_for(lock,
                  std::chrono::milliseconds(m   ? 45
                                            : w ? 15000
                                                : 60000),
                  [&] { return quit || m != doMeter || w != doWeather; });
    }
  }
  if (SUCCEEDED(com))
    CoUninitialize();
}
std::wstring Environment::report() const {
  return status;
}
void Environment::update(Scene &s, HWND fg, ms rt) {
  if (!initClock) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    clock.start(rt, st.wHour * 3600 + st.wMinute * 60 + st.wSecond);
    initClock = true;
    nap = clock.nap(rt);
  }
  bool shallow = clock.nap(rt);
  if (nap && !shallow)
    s.stretch();
  nap = s.shallow = shallow;
  if (rt >= powerAt) {
    powerAt = rt + 220;
    SYSTEM_POWER_STATUS ps{};
    if (GetSystemPowerStatus(&ps) && ps.ACLineStatus <= 1) {
      bool low = ps.ACLineStatus == 0 && ps.BatteryLifePercent <= 20;
      if (low && !s.lowBattery) {
        s.talk(L"Master，电脑好像累了……", 5000);
        s.intents.event(lowEnergy, s.now);
      }
      if (ac == 0 && ps.ACLineStatus == 1)
        s.talk(L"充上电啦！", 3500);
      s.lowBattery = low;
      ac = ps.ACLineStatus;
    }
  }
  if (s.opt.ime && rt >= imeAt) {
    imeAt = rt + 220;
    auto m = imeMode(fg);
    if (m) {
      if ((s.imeKnown && s.imeChinese != *m) || (!s.imeKnown && *m)) {
        s.talk(*m ? L"写代码吗？" : L"嗯，我安静看着。", 2200);
        s.sound(Sound::ime, -18);
      }
      s.imeKnown = true;
      s.imeChinese = *m;
    } else
      s.imeKnown = false;
  } else if (!s.opt.ime)
    s.imeKnown = false;
  if (s.opt.gamma && rt >= gammaAt) {
    gammaAt = rt + 3000;
    auto k = whitePoint(fg);
    double a = k && *k <= 4000 ? 1. : 0., b = k && *k >= 6500 ? 1. : 0.;
    if (a != toWarm || b != toCold) {
      transition = rt;
      fromWarm = s.warm;
      fromCold = s.cold;
      toWarm = a;
      toCold = b;
    }
  }
  double blend = ease((rt - transition) / 1500.);
  s.warm = s.opt.gamma ? fromWarm + (toWarm - fromWarm) * blend : 0;
  s.cold = s.opt.gamma ? fromCold + (toCold - fromCold) * blend : 0;
  Reading a;
  {
    std::lock_guard<std::mutex> lock(mu);
    a = rr;
  }
  if (s.opt.meter && a.meterOK)
    s.beat.sample(rt, a.peak, a.muted);
  else
    s.beat.clear();
  s.rain = s.opt.weather &&
           ((a.weather >= 51 && a.weather <= 67) || (a.weather >= 80 && a.weather <= 82) || a.weather >= 95);
  s.sun = s.opt.weather && a.weather == 0;
  status = s.opt.weather
               ? (a.weather < 0 ? L"天气：无本机天气源"
                                : L"天气：" + wide(a.city) + L" · WMO " + std::to_wstring(a.weather))
               : L"天气：关闭";
  status += s.shallow ? L"　｜　晨间浅眠" : L"　｜　清醒";
  if (s.opt.meter)
    status += a.meterOK ? L"　｜　峰值 " + std::to_wstring(int(a.peak * 100)) + L"%" : L"　｜　无可用音频表";
}
} // namespace pet

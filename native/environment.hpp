#pragma once
#include "scene.hpp"
#include "vault.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
namespace pet {
class Environment {
  struct Reading {
    double peak = 0;
    bool muted = false, meterOK = false;
    int weather = -1;
    std::string city;
    sec forecastAt = 0;
  };
  Vault &files;
  std::mutex mu;
  std::condition_variable cv;
  Reading rr;
  std::atomic<bool> quit{false}, doMeter{false}, doWeather{false};
  std::thread worker;
  BioClock clock;
  ms powerAt = 0, imeAt = 0, gammaAt = 0, last = 0, transition = 0;
  int ac = -1;
  bool initClock = false, nap = false;
  double fromWarm = 0, fromCold = 0, toWarm = 0, toCold = 0;
  void loop();
  bool prevI = false, prevG = false;

public:
  explicit Environment(Vault &v) : files(v) {}
  ~Environment();
  void configure(const Options &, bool paused);
  void update(Scene &, HWND foreground, ms monotonic);
  std::wstring report() const;
  std::wstring status = L"未启用额外感知";
};
} // namespace pet

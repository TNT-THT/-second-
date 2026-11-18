#pragma once
#include "vault.hpp"
#include "scene.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <mmsystem.h>
namespace pet {
class Memo {
  Vault &v;
  HWAVEIN mic = nullptr;
  WAVEHDR hdr{};
  std::vector<int16_t> pcm;
  void *ctx = nullptr;
  std::thread worker;
  std::atomic<bool> busy{false}, abort{false}, loaded{false};
  std::mutex lock;
  int result = 0;
  std::string digest;
  ms at = 0;
  void finish(bool transcribe);

public:
  explicit Memo(Vault &f) : v(f) {}
  ~Memo();
  static bool supported();
  void arm();
  void cancel();
  bool begin();
  void tick(Scene &, bool held);
  bool recording() const {
    return mic != nullptr;
  }
};
} // namespace pet

#pragma once
#include "scene.hpp"
#include "vault.hpp"
#include <mmsystem.h>
#include <sapi.h>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace pet {
class Audio {
  Vault &files;
  HWAVEOUT dev = nullptr;
  WAVEHDR hdr{};
  std::vector<int16_t> pcm;
  ISpVoice *voice = nullptr;
  bool queued = false;
  ULONGLONG at = 0;

public:
  explicit Audio(Vault &v) : files(v) {}
  ~Audio();
  void stop();
  void play(Signal);
  double mouth() const;
};
class Effects {
  Vault &files;
  std::mutex lock;
  std::condition_variable cv;
  bool quit = false;
  std::thread worker;
  Fx want = Fx::none, done = Fx::none;
  int frame = -1, published = -1;
  std::array<uint32_t, 64 * 64> image{};
  void loop();

public:
  explicit Effects(Vault &v) : files(v) {}
  ~Effects();
  void request(Fx, int);
  bool copy(Fx, std::array<uint32_t, 4096> &);
};
} // namespace pet

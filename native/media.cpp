#include "media.hpp"
#include <array>
namespace pet {
static const wchar_t *soundFile(Sound s) {
  switch (s) {
  case Sound::ime:
    return L"ime.pcm";
  case Sound::chime:
    return L"chime.pcm";
  case Sound::satisfied:
    return L"satisfied.pcm";
  case Sound::sigh:
    return L"sigh.pcm";
  case Sound::sob:
    return L"sob.pcm";
  case Sound::steps:
    return L"steps.pcm";
  case Sound::birthday:
    return L"birthday.pcm";
  case Sound::cracker:
    return L"cracker.pcm";
  default:
    return L"";
  }
}
void Audio::stop() {
  if (voice)
    voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
  if (queued && dev) {
    waveOutReset(dev);
    waveOutUnprepareHeader(dev, &hdr, sizeof hdr);
    queued = false;
  }
}
Audio::~Audio() {
  stop();
  if (dev)
    waveOutClose(dev);
  if (voice)
    voice->Release();
}
void Audio::play(Signal s) {
  if (s.sound == Sound::weekly) {
    if (!voice)
      CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&voice));
    if (voice) {
      voice->SetVolume(USHORT(std::round(100 * std::pow(10., s.db / 20))));
      voice->Speak(L"这周辛苦啦", SPF_ASYNC | SPF_IS_NOT_XML, nullptr);
    }
    return;
  }
  if (s.sound == Sound::steps && queued && !(hdr.dwFlags & WHDR_DONE))
    return;
  auto b = files.asset(soundFile(s.sound), 44100 * 16);
  if (b.empty() || b.size() % 2)
    return;
  if (!dev) {
    WAVEFORMATEX fmt{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    if (waveOutOpen(&dev, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
      dev = nullptr;
      return;
    }
  }
  if (queued) {
    if (!(hdr.dwFlags & WHDR_DONE))
      waveOutReset(dev);
    waveOutUnprepareHeader(dev, &hdr, sizeof hdr);
    queued = false;
  }
  pcm.resize(b.size() / 2);
  double amp = std::pow(10., s.db / 20);
  for (size_t i = 0; i < pcm.size(); ++i)
    pcm[i] = int16_t(std::lround(int16_t(b[2 * i] | (int(b[2 * i + 1]) << 8)) * amp));
  hdr = {};
  hdr.lpData = reinterpret_cast<char *>(pcm.data());
  hdr.dwBufferLength = DWORD(pcm.size() * 2);
  if (waveOutPrepareHeader(dev, &hdr, sizeof hdr) != MMSYSERR_NOERROR)
    return;
  if (waveOutWrite(dev, &hdr, sizeof hdr) != MMSYSERR_NOERROR) {
    waveOutUnprepareHeader(dev, &hdr, sizeof hdr);
    return;
  }
  queued = true;
  at = GetTickCount64();
}
double Audio::mouth() const {
  if (!queued || (hdr.dwFlags & WHDR_DONE))
    return 0;
  size_t i = size_t((GetTickCount64() - at) * 441 / 10);
  double sum = 0;
  size_t n = 0;
  for (; i < pcm.size() && n < 441; i += 7, n += 7)
    sum += std::abs(int(pcm[i]));
  return n ? std::min(1., sum * 7 / (n * 5000.)) : 0;
}
void Effects::request(Fx f, int n) {
  std::lock_guard<std::mutex> g(lock);
  if (f == want && n == frame)
    return;
  want = f;
  frame = n;
  if (!worker.joinable() && f != Fx::none)
    worker = std::thread(&Effects::loop, this);
  cv.notify_one();
}
bool Effects::copy(Fx f, std::array<uint32_t, 4096> &out) {
  std::lock_guard<std::mutex> g(lock);
  if (done != f || published < 0)
    return false;
  out = image;
  return true;
}
Effects::~Effects() {
  {
    std::lock_guard<std::mutex> g(lock);
    quit = true;
    cv.notify_one();
  }
  if (worker.joinable())
    worker.join();
}
void Effects::loop() {
  Fx cached = Fx::none;
  std::vector<uint8_t> bytes;
  std::unique_lock<std::mutex> g(lock);
  for (;;) {
    cv.wait(g, [&]() { return quit || want != done || frame != published; });
    if (quit)
      return;
    auto f = want;
    int n = frame;
    g.unlock();
    if (f != cached) {
      const wchar_t *name = f == Fx::star      ? L"fx-star.bgra"
                            : f == Fx::flowers ? L"fx-flowers.bgra"
                            : f == Fx::heart   ? L"fx-heart.bgra"
                                               : L"fx-lantern.bgra";
      bytes = f == Fx::none ? std::vector<uint8_t>{} : files.asset(name, 64 * 64 * 4 * 12);
      cached = f;
    }
    std::array<uint32_t, 4096> staged{};
    if (n >= 0 && size_t(n + 1) * sizeof(staged) <= bytes.size())
      memcpy(staged.data(), bytes.data() + n * sizeof(staged), sizeof(staged));
    g.lock();
    image = staged;
    published = n;
    done = f;
  }
}
} // namespace pet

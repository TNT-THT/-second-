#include "memo.hpp"
#include <cmath>
#ifdef PET_WHISPER
#include <whisper.h>
#endif
namespace pet {
bool Memo::supported() {
#ifdef PET_WHISPER
  return true;
#else
  return false;
#endif
}
void Memo::arm() {
#ifdef PET_WHISPER
  if (busy || loaded)
    return;
  if (worker.joinable())
    worker.join();
  abort = false;
  busy = true;
  worker = std::thread([this] {
    try {
      auto weights = v.asset(L"whisper-tiny.bin", 80 * 1024 * 1024);
      if (weights.empty() || sha256({(const char *)weights.data(), weights.size()}) != PET_WHISPER_HASH)
        throw 1;
      auto p = whisper_context_default_params();
      p.use_gpu = false;
      p.flash_attn = true;
      auto context = whisper_init_from_buffer_with_params(weights.data(), weights.size(), p);
      if (!context)
        throw 1;
      if (abort || whisper_model_n_audio_layer(context) != 4 || whisper_model_n_text_layer(context) != 4) {
        whisper_free(context);
        throw 1;
      }
      ctx = context;
      loaded = true;
    } catch (...) {
      std::lock_guard<std::mutex> g(lock);
      result = -1;
    }
    busy = false;
  });
#endif
}
bool Memo::begin() {
  if (!loaded || busy || mic)
    return false;
  if (worker.joinable())
    worker.join();
  abort = false;
  pcm.assign(16000 * 10, 0);
  WAVEFORMATEX fmt{WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0};
  if (waveInOpen(&mic, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
    mic = nullptr;
    return false;
  }
  hdr = {};
  hdr.lpData = (char *)pcm.data();
  hdr.dwBufferLength = DWORD(pcm.size() * 2);
  if (waveInPrepareHeader(mic, &hdr, sizeof hdr) != MMSYSERR_NOERROR ||
      waveInAddBuffer(mic, &hdr, sizeof hdr) != MMSYSERR_NOERROR || waveInStart(mic) != MMSYSERR_NOERROR) {
    finish(false);
    return false;
  }
  at = GetTickCount64();
  return true;
}
void Memo::finish(bool transcribe) {
  if (!mic)
    return;
  waveInStop(mic);
  waveInReset(mic);
  waveInUnprepareHeader(mic, &hdr, sizeof hdr);
  waveInClose(mic);
  mic = nullptr;
  auto n = std::min(pcm.size(), size_t(hdr.dwBytesRecorded / 2));
  std::vector<float> samples;
  if (transcribe && n >= 3200) {
    samples.resize(n);
    for (size_t i = 0; i < n; i++)
      samples[i] = pcm[i] / 32768.f;
  }
  SecureZeroMemory(pcm.data(), pcm.size() * sizeof(int16_t));
  pcm.clear();
#ifdef PET_WHISPER
  if (!samples.empty() && ctx && !abort) {
    busy = true;
    worker = std::thread([this, a = std::move(samples)]() mutable {
      std::string text, key;
      int kind = -1;
      try {
        auto p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        p.n_threads = 1;
        p.language = "zh";
        p.no_context = true;
        p.single_segment = true;
        p.print_special = p.print_progress = p.print_realtime = p.print_timestamps = false;
        p.no_timestamps = true;
        p.max_tokens = 64;
        p.audio_ctx = 512;
        p.abort_callback = [](void *state) { return static_cast<Memo *>(state)->abort.load(); };
        p.abort_callback_user_data = this;
        auto c = static_cast<whisper_context *>(ctx);
        if (!abort && whisper_full(c, p, a.data(), int(a.size())) == 0 && !abort) {
          for (int i = 0; i < whisper_full_n_segments(c); ++i)
            text += whisper_full_get_segment_text(c, i);
          if (!text.empty()) {
            key = sha256(text);
            for (auto &ch : text)
              if (ch >= 'a' && ch <= 'z')
                ch -= 32;
            kind = text.find("AC") != text.npos ? 2 : text.find("WA") != text.npos ? 3 : 1;
          }
        }
      } catch (...) {
      }
      SecureZeroMemory(a.data(), a.size() * sizeof(float));
      if (!text.empty())
        SecureZeroMemory(text.data(), text.size());
      {
        std::lock_guard<std::mutex> g(lock);
        if (!abort) {
          result = kind;
          digest = key;
        }
      }
      busy = false;
    });
  }
#else
  if (!samples.empty())
    SecureZeroMemory(samples.data(), samples.size() * sizeof(float));
#endif
}
void Memo::cancel() {
  abort = true;
  finish(false);
  std::lock_guard<std::mutex> g(lock);
  result = 0;
  digest.clear();
}
void Memo::tick(Scene &s, bool held) {
  if (mic && (!held || GetTickCount64() - at >= 10000))
    finish(true);
  int kind;
  std::string hash;
  {
    std::lock_guard<std::mutex> g(lock);
    kind = result;
    result = 0;
    hash.swap(digest);
  }
  if (!s.opt.voice || abort)
    return;
  if (kind < 0) {
    s.talk(L"这次没听清，麦克风已关闭。");
    return;
  }
  if (kind > 0) {
    std::string record = "{\"hash\":\"" + hash + "\"}";
    auto prior = v.notes(Note::memo, 1024);
    if (std::find(prior.begin(), prior.end(), record) != prior.end()) {
      s.talk(L"这句话我记得。");
      return;
    }
    if (v.note(Note::memo, record))
      s.memo(kind == 2, kind == 3);
    else
      s.talk(L"好像做了个奇怪的梦呢");
  }
}
Memo::~Memo() {
  cancel();
  if (worker.joinable())
    worker.join();
#ifdef PET_WHISPER
  if (ctx)
    whisper_free(static_cast<whisper_context *>(ctx));
#endif
}
} // namespace pet

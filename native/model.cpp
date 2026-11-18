#include "model.hpp"
#include "haru_lock.hpp"
#include <wincodec.h>
namespace pet {
namespace {
template <class T> struct Com {
  T *p = nullptr;
  ~Com() {
    if (p)
      p->Release();
  }
  T **put() {
    return &p;
  }
  T *operator->() const {
    return p;
  }
};
bool png(const std::vector<uint8_t> &bytes, Texture &tex, size_t &budget) {
  if (bytes.size() < 24 || memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8))
    return false;
  Com<IWICImagingFactory> f;
  Com<IWICStream> s;
  Com<IWICBitmapDecoder> d;
  Com<IWICBitmapFrameDecode> img;
  Com<IWICFormatConverter> out;
  Com<IWICBitmapScaler> scaled;
  UINT w = 0, h = 0;
  if (FAILED(
          CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(f.put()))) ||
      FAILED(f->CreateStream(s.put())) ||
      FAILED(s->InitializeFromMemory(const_cast<BYTE *>(bytes.data()), DWORD(bytes.size()))) ||
      FAILED(f->CreateDecoderFromStream(s.p, nullptr, WICDecodeMetadataCacheOnDemand, d.put())) ||
      FAILED(d->GetFrame(0, img.put())) || FAILED(img->GetSize(&w, &h)) || !w || !h || w > 2048 || h > 2048 ||
      FAILED(f->CreateBitmapScaler(scaled.put())) ||
      FAILED(scaled->Initialize(img.p, std::min(w, 1024u), std::min(h, 1024u),
                                WICBitmapInterpolationModeFant)) ||
      FAILED(f->CreateFormatConverter(out.put())) ||
      FAILED(out->Initialize(scaled.p, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                             WICBitmapPaletteTypeCustom)))
    return false;
  w = std::min(w, 1024u);
  h = std::min(h, 1024u);
  if (uint64_t(w) * h * 4 > budget)
    return false;
  tex.w = int(w);
  tex.h = int(h);
  tex.rgba.resize(w * h);
  budget -= w * h * 4;
  return SUCCEEDED(
      out->CopyPixels(nullptr, 4 * w, UINT(4 * w * h), reinterpret_cast<BYTE *>(tex.rgba.data())));
}
} // namespace
bool Model::load(Vault &v) {
  ModelResources r;
  r.original = [&](std::string_view path, size_t cap) { return v.modelAsset(path, cap); };
  r.texture = [&](std::string_view path, Texture &t, size_t &budget) {
    return png(v.modelAsset(path, 8 * 1024 * 1024), t, budget);
  };
  r.tracks = v.asset(L"haru-motion.lut", 2 * 1024 * 1024);
  r.mapping = v.setting(L"intent_mapping.json");
  return actor.load(r);
}
bool Model::verify(Vault &v) {
  auto bytes = v.modelAsset("Haru.moc3", 1024 * 1024);
  bool ok = actor.intact() && !bytes.empty() && sha256({(char *)bytes.data(), bytes.size()}) == haruSha;
  if (!ok)
    actor.discard();
  return ok;
}
} // namespace pet

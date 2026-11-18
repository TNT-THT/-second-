#include "avatar.hpp"
#include "haru_lock.hpp"
#ifdef PET_OFFICIAL_CORE
#include <Live2DCubismCore.h>
#else
#include "PurismCoreBundle.h"
#endif
#include "json.hpp"
#include <numeric>
#include <unordered_map>
namespace pet {
namespace {
struct MeshPoint {
  double x, y, u, v;
};
inline int channel(uint32_t c, int s) {
  return (c >> s) & 255;
}
} // namespace
struct Avatar::Impl {
  void *mocMem = nullptr, *modelMem = nullptr;
  csmModel *m = nullptr;
  csmVector2 size{}, origin{};
  float unit = 1;
  std::vector<Texture> atlas;
  std::vector<int> order;
  std::unordered_map<std::string, int> params;
  std::vector<uint8_t> mask;
  size_t mocSize = 0;
  std::string baseline;
  std::unordered_map<std::string, int> channels;
  std::unordered_map<std::string, bool> physics;
  std::vector<bool> cloth;
  struct Track {
    int index;
    bool part;
    std::vector<float> values;
  };
  std::vector<std::vector<Track>> motions;
  int part(const std::string &s) {
    for (int i = 0; i < csmGetPartCount(m); ++i)
      if (s == csmGetPartIds(m)[i])
        return i;
    return -1;
  }
  bool decodeTracks(const std::vector<uint8_t> &b) {
    size_t n = 4;
    auto take = [&](void *dst, size_t len) {
      if (n + len > b.size())
        throw std::runtime_error("tracks");
      memcpy(dst, b.data() + n, len);
      n += len;
    };
    auto u32 = [&] {
      uint32_t v;
      take(&v, 4);
      return v;
    };
    if (b.size() < 8 || memcmp(b.data(), "PDTR", 4) || u32() != 4)
      return false;
    for (int i = 0; i < 4; i++) {
      auto count = u32(), frames = u32();
      if (count > 256 || frames < 2 || frames > 4000)
        return false;
      motions.emplace_back();
      for (unsigned j = 0; j < count; j++) {
        uint8_t len, isPart;
        take(&len, 1);
        take(&isPart, 1);
        if (!len || len > 80 || isPart > 1)
          return false;
        std::string key(len, ' ');
        take(key.data(), len);
        int ix = isPart ? part(key) : params.count(key) ? params[key] : -1;
        Track t{ix, isPart != 0, std::vector<float>(frames)};
        take(t.values.data(), frames * 4);
        if (ix >= 0)
          motions.back().push_back(std::move(t));
      }
    }
    return n == b.size();
  }
  ~Impl() {
    ::operator delete(modelMem, std::align_val_t(16));
    ::operator delete(mocMem, std::align_val_t(64));
  }
  bool open(ModelResources &v) {
    auto bytes = v.original("Haru.model3.json", 256 * 1024);
    if (bytes.empty())
      return false;
    auto j = nlohmann::json::parse(bytes);
    if (j.at("Version") != 3)
      return false;
    auto &refs = j.at("FileReferences");
    auto textures = refs.at("Textures");
    if (!textures.is_array() || textures.empty() || textures.size() > 4)
      return false;
    auto buf = v.original(refs.at("Moc").get<std::string>(), 8 * 1024 * 1024);
    if (buf.size() < 64)
      return false;
    if (sha256({(const char *)buf.data(), buf.size()}) != haruSha)
      return false;
    auto n = unsigned(buf.size());
    mocSize = n;
    mocMem = ::operator new(n, std::align_val_t(64), std::nothrow);
    if (!mocMem)
      return false;
    memcpy(mocMem, buf.data(), n);
    std::vector<uint8_t>().swap(buf);
    auto ver =
        csmGetMocVersion(mocMem, n); // 4-r.7 ships Haru with the backward-compatible 3.0 binary encoding.
    if (ver < 1 || ver > 4 || !csmHasMocConsistency(mocMem, n))
      return false;
    auto moc = csmReviveMocInPlace(mocMem, n);
    if (!moc)
      return false;
    auto sz = csmGetSizeofModel(moc);
    if (!sz || sz > 16 * 1024 * 1024)
      return false;
    modelMem = ::operator new(sz, std::align_val_t(16), std::nothrow);
    if (!modelMem || !(m = csmInitializeModelInPlace(moc, modelMem, sz)))
      return false;
    int count = csmGetDrawableCount(m);
    if (count < 1 || count > 1024)
      return false;
    order.resize(count);
    std::iota(order.begin(), order.end(), 0);
    size_t budget = 32 * 1024 * 1024;
    for (auto &t : textures) {
      atlas.emplace_back();
      if (!v.texture(t.get<std::string>(), atlas.back(), budget))
        return false;
    }
    int triangles = 0;
    for (int i : order) {
      int tex = csmGetDrawableTextureIndices(m)[i];
      if (tex < 0 || size_t(tex) >= atlas.size() || csmGetDrawableMaskCounts(m)[i] > 32)
        return false;
      triangles += csmGetDrawableIndexCounts(m)[i] / 3;
    }
    if (triangles > 40000)
      return false;
    for (int i = 0; i < csmGetParameterCount(m); ++i)
      params[csmGetParameterIds(m)[i]] = i;
    baseline = sha256({(const char *)mocMem, mocSize});
    auto phys = v.original("Haru.physics3.json", 65536);
    if (!phys.empty()) {
      auto doc = nlohmann::json::parse(phys);
      for (auto &group : doc.at("PhysicsSettings"))
        for (auto &o : group.at("Output"))
          physics[o.at("Destination").at("Id").get<std::string>()] = true;
    }
    auto normalize = [](std::string s) {
      auto split = s.find_last_of(":/");
      if (split != s.npos)
        s.erase(0, split + 1);
      std::string k;
      for (char c : s) {
        if (c >= 'A' && c <= 'Z')
          c += 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
          k += c;
      }
      if (k.rfind("parameter", 0) == 0)
        k.erase(0, 9);
      else if (k.rfind("param", 0) == 0)
        k.erase(0, 5);
      return k;
    };
    auto spec = nlohmann::json::parse(v.mapping);
    for (auto &item : spec.at("channels").items())
      for (auto &alias : item.value()) {
        auto key = alias.get<std::string>();
        auto it = params.find(key);
        if (it == params.end()) {
          auto normalized = normalize(key);
          for (auto cand = params.begin(); cand != params.end(); ++cand)
            if (normalize(cand->first) == normalized) {
              if (it != params.end()) {
                it = params.end();
                break;
              }
              it = cand;
            }
        }
        if (it != params.end() &&
            (!(item.key() == "fringe" || item.key() == "sidelock" || item.key() == "skirt") ||
             physics.count(it->first))) {
          channels[item.key()] = it->second;
          break;
        }
      }
    if (!decodeTracks(v.tracks))
      return false;
    cloth.resize(count);
    auto parent = csmGetDrawableParentPartIndices(m);
    for (int i : order)
      cloth[i] = parent[i] == part("Part01Body001");
    csmReadCanvasInfo(m, &size, &origin, &unit);
    return size.Y > 0 && size.X > 0 && unit > 0;
  }
  void set(const char *name, double val) {
    auto it = params.find(name);
    if (it == params.end())
      return;
    int i = it->second;
    csmGetParameterValues(m)[i] =
        std::clamp(float(val), csmGetParameterMinimumValues(m)[i], csmGetParameterMaximumValues(m)[i]);
  }
  void pose(const Pose &p, double voice) {
    auto vals = csmGetParameterValues(m);
    auto min = csmGetParameterMinimumValues(m), max = csmGetParameterMaximumValues(m);
    std::copy_n(csmGetParameterDefaultValues(m), csmGetParameterCount(m), vals);
    auto parts = csmGetPartOpacities(m);
    auto write = [&](int i, float v) {
      if (i >= 0 && std::isfinite(v))
        vals[i] = std::clamp(v, min[i], max[i]);
    };
    auto put = [&](const char *channel, double v) {
      auto it = channels.find(channel);
      if (it != channels.end())
        write(it->second, float(v));
    };
    auto &motion = motions[std::clamp(p.track, 0, 3)];
    for (auto &t : motion) {
      size_t frames = t.values.size() - 1;
      uint64_t phase = uint64_t(std::max<ms>(0, p.trackTime)) * 30;
      size_t at = size_t(phase / 1000 % frames);
      float f = float(phase % 1000) * .001f;
      float v = t.values[at] + (t.values[at + 1] - t.values[at]) * f;
      if (t.part)
        parts[t.index] = std::clamp(v, 0.f, 1.f);
      else
        write(t.index, v);
    }
    put("eye_left", p.eye);
    put("eye_right", p.eye);
    put("gaze_x", p.gx / 20 * p.gazeGain);
    put("gaze_y", -p.gy / 20 * p.gazeGain);
    put("breath", p.breath);
    put("smile_left", p.eyesSmile);
    put("smile_right", p.eyesSmile);
    put("body_roll", p.lean + p.step * 3);
    put("body_yaw", p.bodyYaw);
    put("head_tilt", p.lean * .55);
    put("nod", p.nod);
    put("mouth_open", std::max(p.jaw / 8, voice));
    put("mouth_form", p.smile - p.surprise);
    set("ParamTere", p.blush);
    set("ParamAngleX", p.gx * .35 + p.back * 28);
    if (p.sit > .5 || p.cup > .3) {
      put("hand_left", 2);
      put("hand_right", 2);
    } else if (p.wave > .1 || p.vsign > .1) {
      put("hand_right", 1);
      set("ParamHandAngleR", p.wave * 12);
    }
    double a = p.tag == Tag::celebrate ? 1 + .3 * p.expression.weight : 1.;
    put("fringe", (p.hair * .10 + p.step * .6) * a);
    put("sidelock", (p.hair * .08 + p.step * .4) * a);
    put("skirt", p.skirt * a);
    csmUpdateModel(m);
  }
  void draw(uint32_t *dst, int W, int H, V waist, double scale, const Pose &p, double voice) {
    pose(p, voice);
    auto ord = csmGetDrawableRenderOrders(m);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return ord[a] < ord[b]; });
    const double fit = 164 * scale / size.Y;
    auto vs = csmGetDrawableVertexPositions(m);
    auto uvs = csmGetDrawableVertexUvs(m);
    auto indices = csmGetDrawableIndices(m);
    auto counts = csmGetDrawableIndexCounts(m);
    auto flags = csmGetDrawableConstantFlags(m);
    auto dyn = csmGetDrawableDynamicFlags(m);
    auto opacity = csmGetDrawableOpacities(m);
    auto masks = csmGetDrawableMasks(m);
    auto nmask = csmGetDrawableMaskCounts(m);
    auto tint = csmGetDrawableMultiplyColors(m);
    auto screen = csmGetDrawableScreenColors(m);
    auto texids = csmGetDrawableTextureIndices(m);
    mask.resize(size_t(W) * H);
    auto mesh = [&](int i, bool asMask, bool clipped, bool inverted) {
      auto &tex = atlas[texids[i]];
      auto point = [&](int k) {
        auto v = vs[i][k], uv = uvs[i][k];
        return MeshPoint{waist.x + (v.X * unit + origin.X - size.X * .5) * fit,
                         waist.y + (-v.Y * unit + origin.Y - size.Y * .45) * fit, uv.X, 1 - uv.Y};
      };
      for (int tri = 0; tri + 2 < counts[i]; tri += 3) {
        auto a = point(indices[i][tri]), b = point(indices[i][tri + 1]), c = point(indices[i][tri + 2]);
        double det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::abs(det) < 1e-7)
          continue;
        auto limit = [](double v, int hi) { return int(std::clamp(v, 0., double(hi))); };
        int l = limit(std::floor(std::min({a.x, b.x, c.x})), W),
            r = limit(std::ceil(std::max({a.x, b.x, c.x})), W),
            t = limit(std::floor(std::min({a.y, b.y, c.y})), H),
            bot = limit(std::ceil(std::max({a.y, b.y, c.y})), H);
        for (int y = t; y < bot; ++y)
          for (int x = l; x < r; ++x) {
            double bx = x + .5 - a.x, by = y + .5 - a.y, u = (bx * (c.y - a.y) - by * (c.x - a.x)) / det,
                   v = ((b.x - a.x) * by - (b.y - a.y) * bx) / det;
            if (u < 0 || v < 0 || u + v > 1)
              continue;
            int tx = limit((a.u + u * (b.u - a.u) + v * (c.u - a.u)) * tex.w, tex.w - 1),
                ty = limit((a.v + u * (b.v - a.v) + v * (c.v - a.v)) * tex.h, tex.h - 1);
            uint32_t col = tex.rgba[size_t(ty) * tex.w + tx];
            int ix = y * W + x;
            double alpha = channel(col, 24) / 255.;
            if (asMask) {
              mask[ix] = uint8_t(mask[ix] + (255 - mask[ix]) * alpha);
              continue;
            }
            alpha *= opacity[i] * (clipped ? (inverted ? 255 - mask[ix] : mask[ix]) / 255. : 1);
            if (alpha <= .001)
              continue;
            auto old = dst[ix];
            double da = channel(old, 24) / 255.;
            bool add = flags[i] & csmBlendAdditive, mul = flags[i] & csmBlendMultiplicative;
            double oa = add || mul ? da : alpha + da * (1 - alpha);
            uint32_t fresh = uint32_t(std::clamp(oa, 0., 1.) * 255 + .5) << 24;
            const double wet = p.rain && cloth[i] ? .92 : 1.;
            double lum = .2126 * channel(col, 16) + .7152 * channel(col, 8) + .0722 * channel(col, 0);
            for (int sh : {0, 8, 16}) {
              double ch = lum + (channel(col, sh) - lum) * p.saturation;
              double mult = sh == 0   ? tint[i].Z
                            : sh == 8 ? tint[i].Y
                                      : tint[i].X,
                     scr = sh == 0   ? screen[i].Z
                           : sh == 8 ? screen[i].Y
                                     : screen[i].X;
              if (ch > 200)
                ch = lum + (ch - lum) * p.highlightSat;
              if (p.hue != 0) {
                double r = channel(col, 16), g = channel(col, 8), b = channel(col, 0);
                ch += (sh == 16 ? b - g : sh == 8 ? r - b : g - r) * (p.hue * .0101);
              }
              ch = std::clamp(wet * (ch * mult + (255 - ch * mult) * scr) * p.light *
                                  (ch > 210 ? p.highlights : 1),
                              0., 255.);
              double dc = channel(old, sh);
              double out = add   ? dc + ch * alpha
                           : mul ? dc * (1 - alpha + alpha * ch / 255)
                                 : ch * alpha + dc * (1 - alpha);
              fresh |= uint32_t(std::clamp(out, 0., oa * 255) + .5) << sh;
            }
            dst[ix] = fresh;
          }
      }
    };
    for (int i : order) {
      if (!(dyn[i] & csmIsVisible) || opacity[i] <= 0)
        continue;
      if (nmask[i]) {
        std::fill(mask.begin(), mask.end(), 0);
        for (int k = 0; k < nmask[i]; ++k)
          mesh(masks[i][k], true, false, false);
      }
      mesh(i, false, nmask[i] != 0, (flags[i] & csmIsInvertedMask) != 0);
    }
    csmResetDrawableDynamicFlags(m);
  }
};
bool Avatar::supports(std::string_view semantic) const {
  return p && p->channels.count(std::string(semantic));
}
Avatar::Avatar() = default;
Avatar::~Avatar() = default;
void Avatar::discard() {
  p.reset();
}
bool Avatar::present() const {
  return bool(p);
}
bool Avatar::load(ModelResources &r) {
  discard();
  try {
    auto next = std::make_unique<Impl>();
    if (next->open(r)) {
      p = std::move(next);
      return true;
    }
  } catch (...) {
  }
  return false;
}
bool Avatar::intact() const {
  return p && sha256({(const char *)p->mocMem, p->mocSize}) == p->baseline;
}
int Avatar::channels() const {
  return p ? int(p->channels.size()) : 0;
}
void Avatar::paint(uint32_t *dst, int w, int h, V v, double scale, const Pose &pose, double mouth) {
  if (p)
    p->draw(dst, w, h, v, scale, pose, mouth);
}
} // namespace pet

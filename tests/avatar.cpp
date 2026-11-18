// Test-only resource adapter. The release has no arbitrary-path model loader or frame export.
#include "avatar.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
using namespace pet;
static std::vector<uint8_t> bytes(const fs::path &p) {
  std::ifstream s(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(s), {}};
}
int main(int argc, char **argv) {
  if (argc < 3 || argc > 6)
    return 2;
  fs::path base = argv[1], scratch = argv[2];
  fs::path model = argc > 3 ? fs::path(argv[3]) : base / "assets/models/haru";
  std::string moc = argc > 4 ? argv[4] : "Haru.moc3", tag = argc > 5 ? argv[5] : "haru";
  fs::path out = scratch / tag;
  fs::create_directories(out);
  ModelResources r;
  r.original = [&](std::string_view p, size_t) { return bytes(model / std::string(p)); };
  r.texture = [&](std::string_view p, Texture &t, size_t &budget) {
    auto b = bytes(out / (fs::path(p).stem().string() + ".bgra"));
    if (b.size() != 1024 * 1024 * 4 || b.size() > budget)
      return false;
    t.w = t.h = 1024;
    t.rgba.resize(1024 * 1024);
    memcpy(t.rgba.data(), b.data(), b.size());
    budget -= b.size();
    return true;
  };
  r.tracks = bytes(base / "assets/haru-motion.lut");
  auto spec = bytes(base / "config/intent_mapping.json");
  r.mapping = {spec.begin(), spec.end()};
  r.manifest = moc.substr(0, moc.size() - 5) + ".model3.json";
  auto mocBytes = bytes(model / moc);
  r.pin = sha256({(const char *)mocBytes.data(), mocBytes.size()});
  Avatar a;
  if (!a.load(r) || a.channels() < 12)
    return 3;
  int W = 320, H = 320;
  std::vector<uint32_t> px(W * H);
  std::string prev;
  for (int i = 0; i < 3; i++) {
    std::fill(px.begin(), px.end(), 0);
    Pose p;
    p.track = i;
    p.trackTime = i * 750 + 400;
    p.gx = i * 7;
    p.eye = i == 2 ? .1 : 1;
    p.jaw = i == 1 ? 6 : 0;
    a.paint(px.data(), W, H, {160, 153}, 1.5, p, 0);
    std::string_view raw((const char *)px.data(), px.size() * 4);
    int n = int(std::count_if(px.begin(), px.end(), [](auto c) { return c >> 24 > 190; }));
    std::string hash = sha256(raw);
    std::cout << "real mesh " << i << ": " << n << " opaque pixels, " << a.channels() << " mapped channels, "
              << hash << "\n";
    if (n < 1500 || hash == prev || !a.intact())
      return 4;
    prev = hash;
    std::ofstream(out / ("pose" + std::to_string(i) + ".bgra"), std::ios::binary)
        .write(raw.data(), raw.size());
  }
  for (auto name : {"eye_left", "eye_right", "smile_left", "smile_right", "mouth_open", "mouth_form",
                    "breath", "body_roll", "body_yaw", "head_tilt", "nod", "gaze_x", "gaze_y", "hand_left",
                    "hand_right", "fringe", "sidelock", "skirt"})
    std::cout << "bind " << name << ": " << (a.supports(name) ? "yes" : "no") << "\n";
  auto old = r.original;
  r.original = [&](auto path, auto cap) {
    auto b = old(path, cap);
    if (path == moc)
      b[300] ^= 1;
    return b;
  };
  if (a.load(r) || a.present())
    return 5;
  return 0;
}

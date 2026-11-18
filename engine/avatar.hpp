#pragma once
#include "scene.hpp"
#include <functional>
#include <memory>
namespace pet {
struct Texture {
  int w = 0, h = 0;
  std::vector<uint32_t> rgba;
};
struct ModelResources {
  std::function<std::vector<uint8_t>(std::string_view, size_t)> original;
  std::function<bool(std::string_view, Texture &, size_t &)> texture;
  std::vector<uint8_t> tracks;
  std::string mapping;
};
class Avatar {
  struct Impl;
  std::unique_ptr<Impl> p;

public:
  Avatar();
  ~Avatar();
  bool load(ModelResources &);
  int channels() const;
  bool intact() const;
  bool present() const;
  bool supports(std::string_view semantic) const;
  void paint(uint32_t *, int, int, V, double, const Pose &, double);
  void discard();
};
} // namespace pet

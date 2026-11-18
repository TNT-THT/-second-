#pragma once
#include "model.hpp"
#include "media.hpp"
namespace pet {
class View {
  HWND hwnd;
  ULONG_PTR gdip = 0;
  HDC dc = nullptr;
  HBITMAP dib = nullptr;
  HGDIOBJ old = nullptr;
  uint32_t *bits = nullptr;
  int w = 0, h = 0;
  double font = 16;
  Model actor;
  Effects fx;
  Vault &files;
  bool newyear = false, pumpkin = false, checked = false;
  ULONGLONG integrityAt = 0;
  Box bubble{};
  void allocate();

public:
  explicit View(HWND, Vault &);
  ~View();
  void metrics();
  void sample(uint32_t *d, int w, int h, V waist, double scale, const Pose &pose) {
    actor.paint(d, w, h, waist, scale, pose, 0);
  }
  bool inBubble(V screen, const Scene &) const;
  V center() const;
  bool supports(std::string_view semantic) const {
    return actor.actor.supports(semantic);
  }
  bool hasActor() const {
    return actor.present();
  }
  void show(Scene &, double mouth
#ifdef PET_DEV
            ,
            bool debug, double fps, size_t kib
#endif
  );
};
} // namespace pet

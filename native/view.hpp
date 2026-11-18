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
  bool newyear = false, pumpkin = false, checked = false, darkTheme = false, preloaded = false;
  ULONGLONG integrityAt = 0;
  Box bubble{};
  bool dragActive = false, dragValid = false;
  V dragAt{};
  void allocate();

public:
  explicit View(HWND, Vault &);
  ~View();
  void metrics();
  // Swaps the rig used by the preview and the desktop actor; returns false and keeps nothing on failure.
  bool loadModel(Vault &, const ModelEntry &);
  bool hasActor() const {
    return actor.present();
  }
  void sample(uint32_t *d, int w, int h, V waist, double scale, const Pose &pose) {
    actor.paint(d, w, h, waist, scale, pose, 0);
  }
  bool inBubble(V screen, const Scene &) const;
  V center() const;
  // Drag cue: a highlight ring for an accepted payload, a translucent mask plus forbidden mark otherwise.
  void dragHint(bool active, bool valid, V at) {
    dragActive = active;
    dragValid = valid;
    dragAt = at;
  }
  bool supports(std::string_view semantic) const {
    return actor.actor.supports(semantic);
  }
  void show(Scene &, double mouth
#ifdef PET_DEV
            ,
            bool debug, double fps, size_t kib
#endif
  );
};
} // namespace pet

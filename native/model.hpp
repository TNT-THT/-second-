#pragma once
#include "avatar.hpp"
#include "models.hpp"
#include "vault.hpp"
namespace pet {
// Fallback used only when no registry entry loaded; identical to the pinned Haru release.
inline ModelEntry haruModelEntry() {
  ModelEntry e;
  e.id = "haru";
  e.name = "Haru";
  e.dir = "assets/models/haru";
  e.moc = "Haru.moc3";
  e.sha = haruSha;
  return e;
}
struct Model {
  Avatar actor;
  std::string mocName = "Haru.moc3", pin = haruSha;
  bool load(Vault &, const ModelEntry &);
  bool verify(Vault &);
  bool present() const {
    return actor.present();
  }
  void paint(uint32_t *d, int w, int h, V v, double scale, const Pose &p, double mouth) {
    actor.paint(d, w, h, v, scale, p, mouth);
  }
};
} // namespace pet

#pragma once
#include "avatar.hpp"
#include "vault.hpp"
namespace pet {
struct Model {
  Avatar actor;
  bool load(Vault &);
  bool verify(Vault &);
  bool present() const {
    return actor.present();
  }
  void paint(uint32_t *d, int w, int h, V v, double scale, const Pose &p, double mouth) {
    actor.paint(d, w, h, v, scale, p, mouth);
  }
};
} // namespace pet

#pragma once
#include "view.hpp"
#include <functional>
namespace pet {
enum class Page { home, intents, senses, doodle, memories };
class Workbench {
  struct Impl;
  std::unique_ptr<Impl> p;

public:
  using Apply = std::function<bool(const Options &)>;
  Workbench(Scene &, Vault &, View &, Apply, std::function<std::wstring()> env,
            std::function<bool()> voiceReady);
  ~Workbench();
  void show(Page);
  bool dialog(MSG &);
  void close();
  // Called after a live rig swap so the workbench can show the measured latency.
  void switched(const std::string &name, unsigned long long ms);
};
} // namespace pet

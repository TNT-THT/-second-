#pragma once
#include "rules.hpp"
#include <windows.h>
#include <functional>
#include <utility>
#include <vector>
namespace pet {
std::string utf8(std::wstring_view);
std::wstring wide(std::string_view);
struct Handle {
  HANDLE h = INVALID_HANDLE_VALUE;
  Handle() = default;
  explicit Handle(HANDLE v) : h(v) {}
  ~Handle() {
    if (*this)
      CloseHandle(h);
  }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  Handle(Handle &&o) noexcept : h(o.h) {
    o.h = INVALID_HANDLE_VALUE;
  }
  Handle &operator=(Handle &&o) noexcept {
    if (this != &o) {
      if (*this)
        CloseHandle(h);
      h = o.h;
      o.h = INVALID_HANDLE_VALUE;
    }
    return *this;
  }
  explicit operator bool() const {
    return h != INVALID_HANDLE_VALUE && h != nullptr;
  }
};
struct FoodMeta {
  std::string hash, type;
  uint64_t size = 0;
  std::wstring path; // canonical path, only used for the consented recycle step
};
std::optional<FoodMeta> inspectFood(std::wstring_view); // attribute query only; never a file handle
enum class Note : unsigned { preferences, doodles, memo, weather, style, models };
class Vault final : public Journal {
  struct Dir {
    Handle guard;
    std::wstring path;
  };
  struct Stream {
    Handle file;
    bool fence = false;
  };
  Dir root, assets, config, data, models;
  Dir model; // the active model directory, selected from the registry
  mutable std::vector<std::pair<std::wstring, Dir>> subdirs;
  std::wstring activeDir;
  Stream state, feed;
  bool healthy = false;
  static Dir directory(std::wstring, bool root = false);
  static Handle readOnly(const Dir &, std::wstring_view);
  static Stream stream(const Dir &, const wchar_t *);
  static std::optional<uint64_t> append(Stream &, const std::string &);
  static std::vector<uint8_t> bytes(const Dir &, std::wstring_view, size_t);
  const Dir *modelDirFor(std::string_view, std::wstring &) const;

public:
  explicit Vault(std::wstring rootPath);
  static std::wstring exeDir();
  void recover(Life &, sec);
  std::optional<uint64_t> meal(const Meal &) override;
  bool snapshot(const State &) override;
  std::vector<uint8_t> asset(std::wstring_view, size_t max) const;
  std::optional<Options> options() const;
  std::vector<uint8_t> modelAsset(std::string_view relative, size_t max) const;
  bool modelHas(std::string_view relative) const; // metadata only, never reads content
  // Resolves a validated registry path ("assets/models/haru", "data/models/mine") relative to the app root.
  bool useModel(std::string_view relative);
  const std::wstring &modelDir() const {
    return activeDir;
  }
  std::string feedLimits() const;
  // Moves a consented snack into the OS recycle bin, keeping "<timestamp>_<name>" for traceability.
  std::wstring recycle(std::wstring_view, std::string_view stamp);
  std::string recycle(std::wstring_view path); // stamps with the local clock
  std::string setting(const wchar_t *name, size_t cap = 65536) const;
  bool note(Note, const std::string &jsonLine);
  std::vector<std::string> notes(Note, size_t limit) const;
};
} // namespace pet

#include "drop.hpp"
#include <new>
#include <objidl.h>
#include <oleidl.h>
#include <shlobj.h>
namespace pet {
namespace {
class Target final : public IDropTarget {
  LONG refs = 1;
  DropHooks hooks;
  bool inside = false;
  ~Target() {
    // RAII: a lost session (cancel, crash path, revoke) still clears the visual state.
    if (inside && hooks.leave)
      hooks.leave();
  }
  void reset() {
    if (inside) {
      inside = false;
      if (hooks.leave)
        hooks.leave();
    }
  }
  static bool files(IDataObject *src, std::vector<std::wstring> &out) {
    FORMATETC fmt{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM st{};
    if (!src || FAILED(src->GetData(&fmt, &st)))
      return false;
    auto drop = static_cast<HDROP>(GlobalLock(st.hGlobal));
    bool ok = false;
    if (drop) {
      auto n = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
      for (UINT i = 0; ok == false && i < n && i < 8; ++i) {
        auto len = DragQueryFileW(drop, i, nullptr, 0);
        if (!len || len > 32760)
          continue;
        std::wstring p(len + 1, L' ');
        if (DragQueryFileW(drop, i, p.data(), len + 1) == len) {
          p.resize(len);
          out.push_back(std::move(p));
          ok = true;
        }
      }
      GlobalUnlock(st.hGlobal);
    }
    ReleaseStgMedium(&st);
    return ok;
  }
  static bool text(IDataObject *src, std::wstring &out) {
    FORMATETC fmt{CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM st{};
    if (!src || FAILED(src->GetData(&fmt, &st)))
      return false;
    auto p = static_cast<const wchar_t *>(GlobalLock(st.hGlobal));
    bool ok = false;
    if (p) {
      std::wstring v(p);
      if (!v.empty() && v.size() <= 4096) {
        out = std::move(v);
        ok = true;
      }
      GlobalUnlock(st.hGlobal);
    }
    ReleaseStgMedium(&st);
    return ok;
  }
  POINT where(DWORD *effect, POINTL pt) const {
    POINT p{pt.x, pt.y};
    if (hooks.cursor)
      p = hooks.cursor();
    (void)effect;
    return p;
  }

public:
  explicit Target(DropHooks h) : hooks(std::move(h)) {}
  IFACEMETHODIMP QueryInterface(REFIID i, void **out) override {
    if (!out)
      return E_POINTER;
    *out = nullptr;
    if (i == IID_IUnknown || i == IID_IDropTarget) {
      *out = static_cast<IDropTarget *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  IFACEMETHODIMP_(ULONG) AddRef() override {
    return ULONG(InterlockedIncrement(&refs));
  }
  IFACEMETHODIMP_(ULONG) Release() override {
    auto n = InterlockedDecrement(&refs);
    if (!n)
      delete this;
    return ULONG(n);
  }
  IFACEMETHODIMP DragEnter(IDataObject *src, DWORD, POINTL pt, DWORD *effect) override {
    if (!effect)
      return E_INVALIDARG;
    auto at = where(effect, pt);
    std::vector<std::wstring> list;
    bool valid = false;
    if (files(src, list))
      valid = hooks.validFile && hooks.overActor && hooks.overActor(at) && hooks.validFile(list.front());
    inside = true;
    if (hooks.hover)
      hooks.hover(valid, at);
    *effect = valid ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
  }
  IFACEMETHODIMP DragOver(DWORD, POINTL pt, DWORD *effect) override {
    if (!effect)
      return E_INVALIDARG;
    auto at = where(effect, pt);
    // Re-evaluate over the actor so the cue follows the cursor; the payload is not read again.
    POINT p{pt.x, pt.y};
    bool over = hooks.overActor && hooks.overActor(hooks.cursor ? hooks.cursor() : p);
    if (hooks.hover && inside)
      hooks.hover(over, at);
    *effect = over ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
  }
  IFACEMETHODIMP DragLeave() override {
    reset();
    return S_OK;
  }
  IFACEMETHODIMP Drop(IDataObject *src, DWORD, POINTL pt, DWORD *effect) override {
    if (!effect)
      return E_INVALIDARG;
    auto at = where(effect, pt);
    bool over = hooks.overActor && hooks.overActor(at);
    std::vector<std::wstring> list;
    std::wstring txt;
    reset();
    *effect = DROPEFFECT_NONE;
    if (!over)
      return S_OK;
    if (files(src, list)) {
      if (hooks.files)
        hooks.files(std::move(list), at);
      *effect = DROPEFFECT_COPY;
    } else if (text(src, txt)) {
      if (hooks.text)
        hooks.text(std::move(txt), at);
      *effect = DROPEFFECT_COPY;
    }
    return S_OK;
  }
};
} // namespace
IDropTarget *makeDropTarget(DropHooks hooks) {
  return new (std::nothrow) Target(std::move(hooks));
}
} // namespace pet

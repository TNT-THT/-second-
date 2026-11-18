#pragma once
// One OLE drop entry for every payload: snacks, model descriptors and doodle text. The shell decides the
// flavour; the app never inspects file contents here.
#include <windows.h>
#include <objidl.h>
#include <oleidl.h>
#include <functional>
#include <string>
#include <vector>
namespace pet {
enum class DropKind { files, text, unknown };
struct DropHooks {
  std::function<POINT()> cursor;                       // current screen cursor
  std::function<bool(POINT)> overActor;                // hit test in screen coordinates
  std::function<bool(const std::wstring &)> validFile; // extension-level acceptance only
  std::function<void(bool valid, POINT)> hover;        // valid ring, or translucent mask + forbidden icon
  std::function<void()> leave;                         // always paired with hover, even on cancel
  std::function<void(std::vector<std::wstring>, POINT)> files;
  std::function<void(std::wstring, POINT)> text;
};
// Reference-counted target; the caller keeps one reference and calls RevokeDragDrop before releasing.
IDropTarget *makeDropTarget(DropHooks hooks);
} // namespace pet

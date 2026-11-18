#include "vault.hpp"
#include "feed.hpp"
#include <algorithm>
#include <array>
#include <shobjidl.h>
#include <wrl/client.h>
#include <cstdio>
using Microsoft::WRL::ComPtr;
namespace pet {
std::string utf8(std::wstring_view w) {
  if (w.empty() || w.size() > 1048576)
    return {};
  int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), int(w.size()), nullptr, 0, nullptr,
                              nullptr);
  std::string s(n, ' ');
  if (n)
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), int(w.size()), s.data(), n, nullptr,
                        nullptr);
  return s;
}
std::wstring wide(std::string_view s) {
  if (s.empty() || s.size() > 1048576)
    return {};
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(n, L' ');
  if (n)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), w.data(), n);
  return w;
}
static bool diskPath(std::wstring_view p) {
  return p.size() >= 3 && ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')) &&
         p[1] == L':' && p[2] == L'\\' && p.find(L':', 2) == p.npos && p.find(L'\0') == p.npos;
}
static bool leaf(std::wstring_view p) {
  return !p.empty() && p.size() <= 200 && p.front() != L'.' && p.back() != L'.' && p.back() != L' ' &&
         p.find_first_of(L"/\\:*?\"<>|") == p.npos &&
         std::all_of(p.begin(), p.end(), [](wchar_t x) { return x >= 32; });
}
std::optional<FoodMeta> inspectFood(std::wstring_view path) {
  if (!diskPath(path) || path.size() > 32760)
    return {};
  auto name = path.substr(path.rfind(L'\\') + 1);
  if (!leaf(name))
    return {};
  auto e = foodType(utf8(name));
  if (!e)
    return {};
  std::wstring p(path);
  std::array<wchar_t, 32768> full{};
  DWORD n = GetFullPathNameW(p.c_str(), DWORD(full.size()), full.data(), nullptr);
  if (!n || n >= full.size())
    return {};
  p.assign(full.data(), n);
  auto drive = p.substr(0, 3);
  if (GetDriveTypeW(drive.c_str()) != DRIVE_FIXED && GetDriveTypeW(drive.c_str()) != DRIVE_REMOVABLE)
    return {};
  // Reject traversal through junctions/reparse points (including cloud recall). Never ask the shell for a
  // thumbnail.
  DWORD bad = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE;
  for (auto i = p.find(L'\\', 3); i != p.npos; i = p.find(L'\\', i + 1)) {
    auto attr = GetFileAttributesW(p.substr(0, i).c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & bad))
      return {};
  }
  WIN32_FILE_ATTRIBUTE_DATA a{};
  if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &a) ||
      (a.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | bad)))
    return {};
  int chars = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, p.data(), int(p.size()), nullptr, 0,
                            nullptr, nullptr, 0);
  if (!chars)
    return {};
  std::wstring lower(chars, L' ');
  if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, p.data(), int(p.size()), lower.data(), chars,
                     nullptr, nullptr, 0))
    return {};
  auto u = utf8(lower);
  if (u.empty())
    return {};
  uint64_t size = (uint64_t(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
  return FoodMeta{fingerprint(u, size,
                              (uint64_t(a.ftLastWriteTime.dwHighDateTime) << 32) |
                                  a.ftLastWriteTime.dwLowDateTime),
                  *e, size, p};
}
static std::wstring finalName(HANDLE h) {
  std::wstring p(32768, L' ');
  auto n = GetFinalPathNameByHandleW(h, p.data(), DWORD(p.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!n || n >= p.size())
    return {};
  p.resize(n);
  return p;
}
static bool samePath(const std::wstring &a, const std::wstring &b) {
  return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
static bool regular(HANDLE h) {
  BY_HANDLE_FILE_INFORMATION i{};
  return GetFileType(h) == FILE_TYPE_DISK && GetFileInformationByHandle(h, &i) &&
         !(i.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT |
                                 FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE)) &&
         i.nNumberOfLinks == 1;
}
Vault::Dir Vault::directory(std::wstring p, bool base) {
  if (p.empty())
    return {};
  Handle h(CreateFileW(p.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  BY_HANDLE_FILE_INFORMATION i{};
  if (!h || !GetFileInformationByHandle(h.h, &i) || (i.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
      !(i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    return {};
  auto real = finalName(h.h);
  if (real.rfind(L"\\\\?\\", 0) != 0 || !diskPath(real.substr(4)) || (!base && !samePath(real, p)))
    return {};
  return {std::move(h), std::move(real)};
}
std::wstring Vault::exeDir() {
  std::wstring p(32768, L' ');
  DWORD n = GetModuleFileNameW(nullptr, p.data(), DWORD(p.size()));
  if (!n || n >= p.size())
    return {};
  p.resize(n);
  auto at = p.rfind(L'\\');
  return at == p.npos ? L"" : p.substr(0, at);
}
Vault::Stream Vault::stream(const Dir &d, const wchar_t *fixed) {
  if (!d.guard)
    return {};
  auto p = d.path + L"\\" + fixed;
  Handle h(CreateFileW(p.c_str(), GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!h || !regular(h.h) || !samePath(p, finalName(h.h)))
    return {};
  LARGE_INTEGER size{}, off{};
  if (!GetFileSizeEx(h.h, &size))
    return {};
  bool cut = false;
  if (size.QuadPart) {
    off.QuadPart = -1;
    char c;
    DWORD n;
    if (!SetFilePointerEx(h.h, off, nullptr, FILE_END) || !ReadFile(h.h, &c, 1, &n, nullptr) || n != 1)
      return {};
    cut = c != '\n';
  }
  return {std::move(h), cut};
}
Vault::Vault(std::wstring path) {
  root = directory(std::move(path), true);
  if (!root.guard)
    return;
  assets = directory(root.path + L"\\assets");
  if (assets.guard) {
    models = directory(assets.path + L"\\models");
  }
  config = directory(root.path + L"\\config");
  auto d = root.path + L"\\data";
  CreateDirectoryW(d.c_str(), nullptr);
  data = directory(d);
  state = stream(data, L"state.jsonl");
  feed = stream(data, L"feed_log.jsonl");
  healthy = bool(state.file) && bool(feed.file);
}
Handle Vault::readOnly(const Dir &d, std::wstring_view name) {
  if (!d.guard || !leaf(name))
    return {};
  auto p = d.path + L"\\" + std::wstring(name);
  Handle h(CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!h || !regular(h.h) || !samePath(p, finalName(h.h)))
    return {};
  return h;
}
std::vector<uint8_t> Vault::bytes(const Dir &d, std::wstring_view name, size_t cap) {
  auto h = readOnly(d, name);
  LARGE_INTEGER n{};
  if (!h || !GetFileSizeEx(h.h, &n) || n.QuadPart <= 0 || uint64_t(n.QuadPart) > cap)
    return {};
  std::vector<uint8_t> b(size_t(n.QuadPart));
  DWORD k;
  if (!ReadFile(h.h, b.data(), DWORD(b.size()), &k, nullptr) || k != b.size())
    return {};
  return b;
}
std::vector<uint8_t> Vault::asset(std::wstring_view n, size_t cap) const {
  return bytes(assets, n, cap);
}
std::optional<Options> Vault::options() const {
  auto b = bytes(config, L"settings.json", 4096);
  if (b.empty())
    return {};
  return decodeOptions({reinterpret_cast<char *>(b.data()), b.size()});
}
static bool readAt(HANDLE h, int64_t off, char *to, DWORD n) {
  LARGE_INTEGER p{};
  p.QuadPart = off;
  DWORD k;
  return SetFilePointerEx(h, p, nullptr, FILE_BEGIN) && ReadFile(h, to, n, &k, nullptr) && k == n;
}
static std::optional<State> lastState(HANDLE h, bool &bad) {
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(h, &size))
    return {};
  std::string rev;
  char buf[16384];
  bool over = false;
  auto line = [&]() -> std::optional<State> {
    if (rev.empty() && !over)
      return {};
    if (!over) {
      std::reverse(rev.begin(), rev.end());
      auto s = decodeState(rev);
      if (s)
        return s;
    }
    bad = true;
    over = false;
    rev.clear();
    return {};
  };
  for (auto end = size.QuadPart; end > 0;) {
    DWORD n = DWORD(std::min<int64_t>(sizeof buf, end));
    end -= n;
    if (!readAt(h, end, buf, n)) {
      bad = true;
      return {};
    }
    for (int k = int(n) - 1; k >= 0; --k) {
      char c = buf[k];
      if (c == '\n') {
        if (auto s = line())
          return s;
      } else if (rev.size() < 4096)
        rev += c;
      else
        over = true;
    }
  }
  return line();
}
void Vault::recover(Life &l, sec now) {
  bool bad = false;
  auto s = state.file ? lastState(state.file.h, bad) : std::nullopt;
  LARGE_INTEGER size{};
  bool ok = healthy && GetFileSizeEx(feed.file.h, &size);
  if (!ok || (s && s->cursor > uint64_t(size.QuadPart)))
    s.reset();
  l.load(s, now);
  std::string line;
  bool over = false;
  char buf[16384];
  auto consume = [&](uint64_t end) {
    if (!line.empty() || over) {
      auto m = over ? std::nullopt : decodeMeal(line);
      if (m)
        l.history(*m, end, now, s.has_value());
      else
        bad = true;
    }
    line.clear();
    over = false;
  };
  for (int64_t off = 0; ok && off < size.QuadPart;) {
    DWORD n = DWORD(std::min<int64_t>(sizeof buf, size.QuadPart - off));
    if (!readAt(feed.file.h, off, buf, n)) {
      ok = healthy = false;
      break;
    }
    for (DWORD i = 0; i < n; ++i) {
      if (buf[i] == '\n')
        consume(off + i + 1);
      else if (line.size() < 512)
        line += buf[i];
      else
        over = true;
    }
    off += n;
  }
  consume(uint64_t(size.QuadPart));
  l.s.cursor = size.QuadPart;
  l.dream |= bad || !ok;
  l.ready(now);
}
std::optional<uint64_t> Vault::append(Stream &s, const std::string &text) {
  if (!s.file || !regular(s.file.h))
    return {};
  auto line = (s.fence ? "\n" : "") + text + "\n";
  DWORD n = 0;
  if (!WriteFile(s.file.h, line.data(), DWORD(line.size()), &n, nullptr) || n != line.size() ||
      !FlushFileBuffers(s.file.h)) {
    s.fence = true;
    return {};
  }
  s.fence = false;
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(s.file.h, &size))
    return {};
  return uint64_t(size.QuadPart);
}
std::optional<uint64_t> Vault::meal(const Meal &m) {
  if (!healthy)
    return {};
  auto n = append(feed, encode(m));
  if (!n)
    healthy = false;
  return n;
}
bool Vault::snapshot(const State &s) {
  if (!healthy)
    return false;
  if (!append(state, encode(s))) {
    healthy = false;
    return false;
  }
  return true;
}
bool Vault::useModel(std::string_view rel) {
  model = {};
  activeDir.clear();
  subdirs.clear();
  if (!root.guard || rel.empty() || rel.size() > 160 || rel.front() == '/' || rel.front() == '\\' ||
      rel.find(':') != rel.npos || rel.find("..") != rel.npos || rel.find('\0') != rel.npos)
    return false;
  auto w = wide(rel);
  if (diskPath(w) && w.size() <= 32760) {
    // An imported model may live outside the app folder; the user picked it explicitly and we only read it.
    auto d = directory(w);
    if (!d.guard)
      return false;
    model = std::move(d);
    activeDir = model.path;
    return true;
  }
  std::wstring path = root.path;
  size_t from = 0;
  int depth = 0;
  while (from <= w.size() && depth < 5) {
    auto at = w.find(L'\\', from);
    auto part = w.substr(from, at == std::wstring::npos ? std::wstring::npos : at - from);
    if (!leaf(part))
      return false;
    path += L"\\" + part;
    ++depth;
    if (at == std::wstring::npos)
      break;
    from = at + 1;
  }
  auto d = directory(path);
  if (!d.guard || depth > 4)
    return false;
  model = std::move(d);
  activeDir = model.path;
  return true;
}
const Vault::Dir *Vault::modelDirFor(std::string_view p, std::wstring &leafName) const {
  if (!model.guard || p.empty() || p.size() > 200 || p.find("..") != p.npos || p.find(':') != p.npos)
    return nullptr;
  auto slash = p.find('/');
  if (slash == std::string::npos) {
    leafName = wide(p);
    return &model;
  }
  auto dirName = wide(p.substr(0, slash));
  auto file = wide(p.substr(slash + 1));
  if (!leaf(dirName) || file.empty() || file.find(L'/') != std::wstring::npos)
    return nullptr;
  for (auto &s : subdirs)
    if (s.first == dirName) {
      leafName = file;
      return &s.second;
    }
  auto d = directory(model.path + L"\\" + dirName);
  if (!d.guard)
    return nullptr;
  subdirs.emplace_back(dirName, std::move(d));
  leafName = file;
  return &subdirs.back().second;
}
std::vector<uint8_t> Vault::modelAsset(std::string_view p, size_t cap) const {
  std::wstring name;
  auto d = modelDirFor(p, name);
  return d ? bytes(*d, name, cap) : std::vector<uint8_t>{};
}
bool Vault::modelHas(std::string_view p) const {
  std::wstring name;
  auto d = modelDirFor(p, name);
  // Existence and regularity only: the handle is opened read-only and nothing is read.
  return d && readOnly(*d, name);
}
std::string Vault::feedLimits() const {
  auto b = bytes(config, L"feed_limits.json", 4096);
  return {b.begin(), b.end()};
}
std::wstring Vault::recycle(std::wstring_view path, std::string_view stamp) {
  if (path.empty() || path.size() > 32760 || !leaf(path.substr(path.rfind(L'\\') + 1)))
    return {};
  const std::wstring name = wide(trashStamp(stamp, utf8(path.substr(path.rfind(L'\\') + 1))));
  if (name.empty())
    return {};
  ComPtr<IFileOperation> op;
  if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(op.GetAddressOf()))))
    return {};
  // Undo keeps the bin entry; no prompts and no UI because the pet must never steal focus.
  op->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NO_UI);
  ComPtr<IShellItem> item;
  if (FAILED(SHCreateItemFromParsingName(std::wstring(path).c_str(), nullptr, IID_PPV_ARGS(item.GetAddressOf()))))
    return {};
  if (FAILED(op->SetName(item.Get(), name.c_str())) || FAILED(op->DeleteItem(item.Get(), nullptr)) ||
      FAILED(op->PerformOperations()))
    return {};
  BOOL aborted = TRUE;
  if (FAILED(op->GetAnyOperationsAborted(&aborted)) || aborted)
    return {};
  return name;
}
std::string Vault::setting(const wchar_t *n, size_t cap) const {
  auto b = bytes(config, n, cap);
  return {b.begin(), b.end()};
}
static constexpr const wchar_t *names[] = {L"preferences.jsonl", L"doodles.jsonl", L"memo_hashes.jsonl",
                                           L"weather.jsonl", L"style.jsonl", L"models.jsonl"};
bool Vault::note(Note k, const std::string &line) {
  if (unsigned(k) >= 6 || line.empty() || line.size() > 2048 || line.find_first_of("\r\n") != line.npos)
    return false;
  auto file = stream(data, names[unsigned(k)]);
  return append(file, line).has_value();
}
std::vector<std::string> Vault::notes(Note k, size_t limit) const {
  std::vector<std::string> lines;
  if (unsigned(k) >= 6)
    return lines;
  auto file = readOnly(data, names[unsigned(k)]);
  LARGE_INTEGER sz{};
  if (!file || !GetFileSizeEx(file.h, &sz))
    return lines;
  std::string v;
  bool over = false;
  auto take = [&] {
    if (!over && !v.empty()) {
      std::reverse(v.begin(), v.end());
      lines.push_back(v);
    }
    v.clear();
    over = false;
  };
  char buf[8192];
  for (auto end = sz.QuadPart; end > 0 && lines.size() < limit;) {
    DWORD n = DWORD(std::min<int64_t>(end, sizeof buf));
    end -= n;
    if (!readAt(file.h, end, buf, n))
      return {};
    for (int i = int(n) - 1; i >= 0 && lines.size() < limit; --i) {
      if (buf[i] == '\n')
        take();
      else if (v.size() < 2048)
        v += buf[i];
      else
        over = true;
    }
  }
  if (lines.size() < limit)
    take();
  std::reverse(lines.begin(), lines.end());
  return lines;
}
std::string localStamp() {
  SYSTEMTIME t{};
  GetLocalTime(&t);
  char b[32];
  std::snprintf(b, sizeof b, "%04u-%02u-%02u %02u:%02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                t.wSecond);
  return b;
}
std::string Vault::recycle(std::wstring_view path) {
  return utf8(recycle(path, localStamp()));
}
} // namespace pet

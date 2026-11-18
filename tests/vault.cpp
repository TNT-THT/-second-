// Windows-only: fresh unique sandbox. Test setup can create brand-new sentinels, never needs cleanup
// privileges.
#include "vault.hpp"
#include <cstdio>
#include <ctime>
using namespace pet;
#define REQUIRE(x)                                                                                           \
  do {                                                                                                       \
    if (!(x)) {                                                                                              \
      std::fprintf(stderr, "line %d: %s (winerr %lu)\n", __LINE__, #x, GetLastError());                      \
      return 1;                                                                                              \
    }                                                                                                        \
  } while (0)
static bool create(std::wstring p, const std::string &bytes) {
  Handle f(CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr));
  DWORD n;
  return f && WriteFile(f.h, bytes.data(), DWORD(bytes.size()), &n, nullptr) && n == bytes.size();
}
static std::string content(std::wstring p) {
  Handle f(CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                       nullptr));
  if (!f)
    return {};
  char b[16384];
  DWORD n = 0;
  if (!ReadFile(f.h, b, sizeof b, &n, nullptr))
    return {};
  return {b, n};
}
int main() {
  wchar_t temp[MAX_PATH]{};
  REQUIRE(GetTempPathW(MAX_PATH, temp) > 0);
  std::wstring root = std::wstring(temp) + L"PetDesk-proof-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                      std::to_wstring(GetTickCount64());
  REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
  auto data = root + L"\\data", st = data + L"\\state.jsonl", log = data + L"\\feed_log.jsonl",
       sent = root + L"\\recipe.CPP";
  REQUIRE(CreateDirectoryW(data.c_str(), nullptr));
  REQUIRE(CreateDirectoryW((root + L"\\config").c_str(), nullptr));
  REQUIRE(CreateDirectoryW((root + L"\\assets").c_str(), nullptr));
  std::string secret = "int main(){ /* do not consume my bytes */ }\r\n";
  REQUIRE(create(sent, secret));
  REQUIRE(create(st, "{interrupted"));
  REQUIRE(create(root + L"\\config\\settings.json", "{\"input_counts\":false}"));
  sec t = std::time(nullptr);
  std::string first;
  {
    Vault v(root);
    Life a(v);
    v.recover(a, t);
    REQUIRE(a.dream && a.s.satiety == 50 && v.options() && !v.options()->input);
    REQUIRE(v.asset(L"..\\recipe.CPP", 1024).empty());
    REQUIRE(v.asset(L"C:\\Windows\\notepad.exe", 1024).empty());
    REQUIRE(!inspectFood(root) && !inspectFood(sent + L":ads.txt") &&
            !inspectFood(L"\\\\example.invalid\\food.txt"));
    // No-sharing read lock: attempts to open GENERIC_READ on the food would fail here.
    Handle lock(CreateFileW(sent.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    REQUIRE(lock);
    auto m = inspectFood(sent);
    REQUIRE(m && m->type == "cpp" && m->hash.size() == 8);
    REQUIRE(a.feed(m->hash, m->type, t) == FoodResult::fresh && a.s.satiety == 70);
    REQUIRE(a.feed(m->hash, m->type, t + 1) == FoodResult::again && a.s.satiety == 70);
    first = content(st);
    REQUIRE(first.rfind("{interrupted\n", 0) == 0);
    auto f = content(log);
    REQUIRE(f.find("recipe") == f.npos && f.find("int main") == f.npos && f.find("cpp") != f.npos);
  }
  REQUIRE(content(sent) == secret);
  {
    Vault v(root);
    Life a(v);
    v.recover(a, t + 2);
    REQUIRE(a.s.satiety == 70 && a.today(t + 2) == 2 && !a.dream);
    REQUIRE(content(st).rfind(first, 0) == 0);
  }
  // Even a hard link under the private data directory must never turn into a write capability.
  auto evil = root + L"\\other";
  REQUIRE(CreateDirectoryW(evil.c_str(), nullptr));
  REQUIRE(CreateDirectoryW((evil + L"\\data").c_str(), nullptr));
  REQUIRE(CreateHardLinkW((evil + L"\\data\\state.jsonl").c_str(), sent.c_str(), nullptr));
  {
    Vault v(evil);
    Life a(v);
    v.recover(a, t);
    REQUIRE(a.dream);
    REQUIRE(a.feed("00000000", "txt", t) == FoodResult::io);
  }
  REQUIRE(content(sent) == secret);
  std::puts("metadata under exclusive lock, no original changes, append-prefix, restart, traversal and "
            "hardlink rejection passed");
}

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "bridge.hpp"
#include "extension_origin.hpp"
#include <algorithm>
#include <charconv>
#include <map>
namespace pet {
bool Bridge::start() {
  if (running)
    return true;
  stop();
  unsigned char bytes[6];
  if (BCryptGenRandom(nullptr, bytes, sizeof bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return false;
  key.clear();
  for (auto b : bytes) {
    key += "0123456789ABCDEF"[b >> 4];
    key += "0123456789ABCDEF"[b & 15];
  }
  running = true;
  worker = std::thread(&Bridge::run, this);
  return true;
}
void Bridge::stop() {
  running = false;
  if (worker.joinable())
    worker.join();
  key.clear();
  std::lock_guard<std::mutex> g(lock);
  queue.clear();
}
std::wstring Bridge::code() const {
  return running ? std::wstring(key.begin(), key.end()) : L"未连接";
}
std::vector<LocalEvent> Bridge::drain() {
  std::vector<LocalEvent> v;
  std::lock_guard<std::mutex> g(lock);
  v.swap(queue);
  return v;
}
void Bridge::run() {
  WSADATA ws{};
  if (WSAStartup(MAKEWORD(2, 2), &ws)) {
    running = false;
    return;
  }
  SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  BOOL exclusive = TRUE;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(38417);
  if (ls == INVALID_SOCKET ||
      setsockopt(ls, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&exclusive, sizeof exclusive) ||
      bind(ls, (sockaddr *)&addr, sizeof addr) || listen(ls, 4))
    running = false;
  while (running) {
    fd_set f;
    FD_ZERO(&f);
    FD_SET(ls, &f);
    timeval tv{0, 150000};
    if (select(0, &f, nullptr, nullptr, &tv) <= 0)
      continue;
    SOCKET cl = accept(ls, nullptr, nullptr);
    if (cl == INVALID_SOCKET)
      continue;
    DWORD timeout = 350;
    setsockopt(cl, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    setsockopt(cl, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
    std::string in;
    char buf[1024];
    size_t h = std::string::npos;
    auto until = GetTickCount64() + 650;
    auto more = [&]() {
      if (in.size() >= 4096 || GetTickCount64() >= until)
        return false;
      int n = recv(cl, buf, sizeof buf, 0);
      if (n <= 0)
        return false;
      in.append(buf, size_t(n));
      return true;
    };
    while ((h = in.find("\r\n\r\n")) == in.npos && more()) {
    }
    bool ok = false;
    if (h != in.npos && h < 3500) {
      size_t at = in.find("\r\n");
      auto req = in.substr(0, at);
      at += 2;
      std::map<std::string, std::string> hdr;
      while (at < h) {
        auto en = in.find("\r\n", at), col = in.find(':', at);
        if (en == in.npos || col >= en)
          break;
        auto name = in.substr(at, col - at);
        for (char &c : name)
          if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
        auto val = col + 1;
        while (val < en && in[val] == ' ')
          ++val;
        if (!hdr.emplace(name, in.substr(val, en - val)).second)
          break;
        at = en + 2;
      }
      if (hdr["origin"] == extensionOrigin && hdr["host"] == "127.0.0.1:38417" && at == h + 2) {
        if (req == "OPTIONS /pet HTTP/1.1")
          ok = true;
        else if (req == "POST /pet HTTP/1.1" && hdr["x-desk-pair"] == key &&
                 !hdr.count("transfer-encoding")) {
          auto sz = hdr["content-length"];
          unsigned len = 0;
          auto conv = std::from_chars(sz.data(), sz.data() + sz.size(), len);
          if (conv.ec == std::errc{} && conv.ptr == sz.data() + sz.size() && len >= 4 && len <= 35) {
            while (in.size() < h + 4 + len && more()) {
            }
            auto body = in.substr(h + 4);
            bool here = body == "here";
            bool ac = body.rfind("ac:", 0) == 0 && body.size() > 3 &&
                      body.find_first_not_of(
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-", 3) == body.npos;
            if (body.size() == len && (here || ac)) {
              std::lock_guard<std::mutex> g(lock);
              if (queue.size() < 16)
                queue.push_back({here ? "" : body.substr(3)});
              ok = true;
            }
          }
        }
      }
    }
    std::string response =
        ok ? std::string("HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: ") + extensionOrigin +
                 "\r\nAccess-Control-Allow-Methods: POST\r\nAccess-Control-Allow-Headers: Content-Type, "
                 "X-Desk-Pair\r\nAccess-Control-Allow-Private-Network: true\r\n"
           : "HTTP/1.1 403 Forbidden\r\n";
    response += "Connection: close\r\nContent-Length: 0\r\nCache-Control: no-store\r\n\r\n";
    send(cl, response.data(), int(response.size()), 0);
    closesocket(cl);
  }
  if (ls != INVALID_SOCKET)
    closesocket(ls);
  WSACleanup();
}
} // namespace pet

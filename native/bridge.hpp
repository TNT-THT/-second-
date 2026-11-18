#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace pet {
struct LocalEvent {
  std::string id;
};
class Bridge {
  std::atomic<bool> running{false};
  std::thread worker;
  std::mutex lock;
  std::vector<LocalEvent> queue;
  std::string key;
  void run();

public:
  ~Bridge() {
    stop();
  }
  bool start();
  void stop();
  std::wstring code() const;
  std::vector<LocalEvent> drain();
};
} // namespace pet

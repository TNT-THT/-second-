#pragma once
// Pure feed policy: size limits and recycle decisions, with no file access of any kind.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace pet {
struct FeedLimits {
  uint64_t maxBytes = 50ull * 1024 * 1024;
  bool recycle = true;
  std::string failedLine = "\u6ca1\u529b\u6c14\u642c\u52a8\u5462\u2026";
  std::string heavyLine = "\u8fd9\u4e2a\u592a\u91cd\u4e86\u5403\u4e0d\u4e0b\u2026";
};
std::optional<FeedLimits> parseFeedLimits(std::string_view);
enum class FeedVerdict { ok, tooBig };
// Only the size attribute participates; the caller must not open the file.
FeedVerdict feedVerdict(uint64_t bytes, const FeedLimits &);
// Traceable stamp for a recycled item: "2026-11-18T11-51-09_name.ext".
std::string trashStamp(std::string_view localIso, std::string_view leaf);
} // namespace pet

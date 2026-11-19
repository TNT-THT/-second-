#include "feed.hpp"
#include "json.hpp"
#include <algorithm>
namespace pet {
namespace {
bool inlineLine(std::string &s) {
  if (s.empty() || s.size() > 120)
    return false;
  for (unsigned char c : s)
    if (c < 0x20 && c != '\t')
      return false;
  return true;
}
} // namespace
std::optional<FeedLimits> parseFeedLimits(std::string_view text) {
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(text);
  } catch (...) {
    return {};
  }
  if (!j.is_object())
    return {};
  FeedLimits f;
  if (j.contains("max_bytes")) {
    if (!j["max_bytes"].is_number_unsigned() && !j["max_bytes"].is_number_integer())
      return {};
    auto v = j["max_bytes"].get<long long>();
    if (v < 1024 || v > (8ll << 30))
      return {};
    f.maxBytes = uint64_t(v);
  }
  if (j.contains("recycle")) {
    if (!j["recycle"].is_boolean())
      return {};
    f.recycle = j["recycle"].get<bool>();
  }
  for (auto k : {"failed_line", "heavy_line"}) {
    if (!j.contains(k))
      continue;
    if (!j[k].is_string())
      return {};
    auto s = j[k].get<std::string>();
    if (!inlineLine(s))
      return {};
    (std::string(k) == "failed_line" ? f.failedLine : f.heavyLine) = s;
  }
  return f;
}
FeedVerdict feedVerdict(uint64_t bytes, const FeedLimits &l) {
  return bytes > l.maxBytes ? FeedVerdict::tooBig : FeedVerdict::ok;
}
std::string trashStamp(std::string_view localIso, std::string_view leaf) {
  std::string s(localIso);
  std::replace(s.begin(), s.end(), ':', '-');
  std::replace(s.begin(), s.end(), ' ', 'T');
  if (s.size() > 19)
    s.resize(19);
  std::string name(leaf);
  if (name.size() > 120)
    name.resize(120);
  return s.empty() ? name : s + '_' + name;
}
} // namespace pet

#pragma once
// Model library description and descriptor-level validation. No file or Core access here.
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace pet {
struct ModelAdapter {
  std::string clothPart; // part name used for wet-hem shading; empty means none
  std::vector<std::pair<std::string, std::string>> expressions; // semantic -> exp3 name
  std::string tracks; // optional baked track file inside the model directory
};
struct ModelEntry {
  std::string id, name, dir; // dir is relative to the app root
  std::string manifest = "Haru.model3.json";
  std::string moc = "Haru.moc3";
  std::string sha; // pinned MOC SHA-256
  std::string license;
  bool builtin = true;
  ModelAdapter adapter;
};
std::optional<ModelAdapter> parseAdapter(std::string_view);
std::optional<std::vector<ModelEntry>> parseModels(std::string_view registry);
struct ModelReport {
  std::vector<std::string> missingFiles;    // referenced files absent from the directory
  std::vector<std::string> missingChannels; // semantic channels this rig cannot drive
  std::vector<std::string> notes;           // e.g. no physics document, no expressions
  std::string fingerprint;
  int channels = 0;
  bool usable() const {
    return missingFiles.empty() && channels >= 4;
  }
};
// `read` returns file text by path relative to the model directory, or nullopt. It must never be used
// for user-supplied files: only inside the registered model directory.
using ModelReader = std::function<std::optional<std::string>(std::string_view)>;
ModelReport inspectModel(const ModelReader &, std::string_view manifestName, std::string_view adapterJson,
                         std::string_view mapping);
} // namespace pet

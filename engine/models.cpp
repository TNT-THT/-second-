#include "models.hpp"
#include "json.hpp"
#include "rules.hpp"
#include <algorithm>
#include <set>
namespace pet {
namespace {
std::string normalize(std::string s) {
  auto split = s.find_last_of(":/");
  if (split != s.npos)
    s.erase(0, split + 1);
  std::string k;
  for (char c : s) {
    if (c >= 'A' && c <= 'Z')
      c += 32;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      k += c;
  }
  if (k.rfind("parameter", 0) == 0)
    k.erase(0, 9);
  else if (k.rfind("param", 0) == 0)
    k.erase(0, 5);
  return k;
}
bool relativeName(std::string_view p, int depth = 1) {
  if (p.empty() || p.size() > 200 || p[0] == '/' || p[0] == '\\' || p.find(':') != p.npos ||
      p.find("..") != p.npos)
    return false;
  return int(std::count(p.begin(), p.end(), '/')) <= depth;
}
std::string stem(std::string_view file) {
  auto base = file.substr(file.find_last_of('/') + 1);
  auto dot = base.rfind('.');
  return std::string(dot == std::string_view::npos ? base : base.substr(0, dot));
}
} // namespace
std::optional<ModelAdapter> parseAdapter(std::string_view text) {
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(text);
  } catch (...) {
    return {};
  }
  if (!j.is_object())
    return {};
  ModelAdapter a;
  if (j.contains("cloth_part")) {
    if (!j["cloth_part"].is_string())
      return {};
    a.clothPart = j["cloth_part"].get<std::string>();
    if (a.clothPart.size() > 80 || a.clothPart.find_first_of("\r\n") != a.clothPart.npos)
      return {};
  }
  if (j.contains("tracks")) {
    if (!j["tracks"].is_string())
      return {};
    a.tracks = j["tracks"].get<std::string>();
    if (!relativeName(a.tracks))
      return {};
  }
  if (j.contains("expressions")) {
    if (!j["expressions"].is_object())
      return {};
    for (auto &e : j["expressions"].items()) {
      if (!e.value().is_string())
        return {};
      auto v = e.value().get<std::string>();
      if (v.empty() || v.size() > 80)
        return {};
      a.expressions.emplace_back(e.key(), v);
    }
  }
  return a;
}
std::optional<std::vector<ModelEntry>> parseModels(std::string_view text) {
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(text);
  } catch (...) {
    return {};
  }
  if (!j.is_object() || !j.contains("models") || !j["models"].is_array() || j["models"].size() > 64)
    return {};
  std::vector<ModelEntry> out;
  std::set<std::string> ids;
  for (auto &m : j["models"]) {
    if (!m.is_object())
      return {};
    ModelEntry e;
    for (auto k : {"id", "name", "dir", "manifest", "moc", "sha256"}) {
      if (!m.contains(k) || !m[k].is_string())
        return {};
      auto v = m[k].get<std::string>();
      if (v.empty() || v.size() > 200 || v.find_first_of("\r\n") != v.npos)
        return {};
      std::string key(k);
      if (key == "id")
        e.id = v;
      else if (key == "name")
        e.name = v;
      else if (key == "dir")
        e.dir = v;
      else if (key == "manifest")
        e.manifest = v;
      else if (key == "moc")
        e.moc = v;
      else
        e.sha = v;
    }
    if (e.sha.size() != 64 || !std::all_of(e.sha.begin(), e.sha.end(), [](char c) {
          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
      return {};
    // Registry entries are either app-relative ("assets/models/...") or an absolute folder the user
    // picked during import. Both are validated again by the reader before any file is opened.
    bool absolute = e.dir.size() >= 3 && e.dir[1] == ':' && (e.dir[2] == '\\' || e.dir[2] == '/');
    if (!absolute && !relativeName(e.dir, 3))
      return {};
    if (absolute && (e.dir.find("..") != e.dir.npos || e.dir.find_first_of("\r\n") != e.dir.npos))
      return {};
    if (absolute && e.dir.size() > 200)
      return {};
    if (m.contains("builtin") && m["builtin"].is_boolean())
      e.builtin = m["builtin"].get<bool>();
    if (m.contains("license") && m["license"].is_string())
      e.license = m["license"].get<std::string>();
    if (m.contains("adapter")) {
      if (!m["adapter"].is_object())
        return {};
      auto a = parseAdapter(m["adapter"].dump());
      if (!a)
        return {};
      e.adapter = *a;
    }
    if (!ids.insert(e.id).second)
      return {};
    out.push_back(std::move(e));
  }
  return out;
}
ModelReport inspectModel(const ModelReader &read, std::string_view manifestName, std::string_view adapterJson,
                         std::string_view mapping) {
  ModelReport r;
  auto raw = read(manifestName);
  if (!raw) {
    r.missingFiles.emplace_back(manifestName);
    return r;
  }
  nlohmann::json man, map;
  try {
    man = nlohmann::json::parse(*raw);
    map = nlohmann::json::parse(mapping);
  } catch (...) {
    r.missingFiles.emplace_back(manifestName);
    return r;
  }
  auto adapter = parseAdapter(adapterJson).value_or(ModelAdapter{});
  auto need = [&](std::string_view p) {
    if (!p.empty() && !read(p))
      r.missingFiles.emplace_back(p);
  };
  auto refs = man.value("FileReferences", nlohmann::json::object());
  auto mocName = refs.value("Moc", std::string());
  need(mocName);
  if (refs.contains("Textures") && refs["Textures"].is_array())
    for (auto &t : refs["Textures"])
      if (t.is_string())
        need(t.get<std::string>());
  auto physics = refs.value("Physics", std::string());
  need(physics);
  if (physics.empty())
    r.notes.emplace_back("no physics document");
  std::set<std::string> expressions;
  if (refs.contains("Expressions") && refs["Expressions"].is_array())
    for (auto &e : refs["Expressions"]) {
      if (e.contains("File") && e["File"].is_string())
        need(e["File"].get<std::string>());
      if (e.contains("Name") && e["Name"].is_string())
        expressions.insert(e["Name"].get<std::string>());
    }
  if (expressions.empty())
    r.notes.emplace_back("no expressions; parameter defaults are used");
  for (auto &[semantic, name] : adapter.expressions)
    if (!expressions.count(name))
      r.notes.emplace_back("adapter expression missing: " + semantic);
  std::set<std::string> params, physicsOut;
  if (auto cdi = read(stem(mocName) + ".cdi3.json")) {
    try {
      auto doc = nlohmann::json::parse(*cdi);
      if (doc.contains("Parameters") && doc["Parameters"].is_array())
        for (auto &p : doc["Parameters"])
          if (p.contains("Id") && p["Id"].is_string())
            params.insert(p["Id"].get<std::string>());
    } catch (...) {
      r.notes.emplace_back("cdi3 is not valid JSON");
    }
  } else
    r.notes.emplace_back("no cdi3; channel binding cannot be verified");
  if (!physics.empty()) {
    if (auto p = read(physics)) {
      try {
        auto doc = nlohmann::json::parse(*p);
        for (auto &group : doc.at("PhysicsSettings"))
          for (auto &o : group.at("Output"))
            physicsOut.insert(o.at("Destination").at("Id").get<std::string>());
      } catch (...) {
        r.notes.emplace_back("physics3 is not valid JSON");
      }
    } else
      r.missingFiles.emplace_back(physics);
  }
  std::set<std::string> normalizedParams;
  for (auto &p : params)
    normalizedParams.insert(normalize(p));
  std::set<std::string> bound;
  if (map.contains("channels") && map["channels"].is_object())
    for (auto &item : map["channels"].items()) {
      if (!item.value().is_array())
        continue;
      const std::string &semantic = item.key();
      bool physical = semantic == "fringe" || semantic == "sidelock" || semantic == "skirt";
      for (auto &alias : item.value()) {
        if (!alias.is_string())
          continue;
        auto name = alias.get<std::string>();
        if (!normalizedParams.count(normalize(name)))
          continue;
        if (physical && !physicsOut.count(name))
          continue;
        bound.insert(semantic);
        break;
      }
    }
  r.channels = int(bound.size());
  std::vector<std::string> names(bound.begin(), bound.end());
  std::sort(names.begin(), names.end());
  std::string joined;
  for (auto &n : names)
    joined += n + "\n";
  r.fingerprint = sha256(joined).substr(0, 16);
  for (auto &n : {"breath", "head_tilt", "fringe", "sidelock", "skirt"})
    if (!bound.count(n))
      r.missingChannels.emplace_back(n);
  return r;
}
} // namespace pet

#include "w2ucem/build/ledger.hpp"

#include <nlohmann/json.hpp>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"

namespace w2ucem {

std::string LedgerEntry::to_json() const {
  nlohmann::json j;
  j["build_id"] = build_id;
  j["template_id"] = template_id;
  j["template_version"] = template_version;
  j["template_hash"] = template_hash;
  j["evaluator_version"] = evaluator_version;
  j["target"] = target;
  j["runner_sha256"] = runner_sha256;
  j["timestamp"] = timestamp;
  j["success"] = success;

  j["sources"] = nlohmann::json::object();
  for (const auto& [id, sha] : sources) {
    j["sources"][id] = sha;
  }
  j["images"] = nlohmann::json::object();
  for (const auto& [img, digest] : images) {
    j["images"][img] = digest;
  }
  j["plugin_hashes"] = plugin_hashes;
  j["artifacts"] = nlohmann::json::object();
  for (const auto& [path, sha] : artifacts) {
    j["artifacts"][path] = sha;
  }
  j["symtabs"] = nlohmann::json::object();
  for (const auto& [path, sha] : symtabs) {
    j["symtabs"][path] = sha;
  }
  j["symtab_tool"] = symtab_tool;
  j["buildenv_ref"] = buildenv_ref;
  j["dep_manifests"] = dep_manifests;
  j["dep_scan_skipped"] = dep_scan_skipped;
  return j.dump(2);
}

Result<LedgerEntry> LedgerEntry::from_json(const std::string& json,
                                           const std::string& context) {
  try {
    const auto j = nlohmann::json::parse(json);
    LedgerEntry e;
    e.build_id = j.at("build_id").get<std::string>();
    e.template_id = j.at("template_id").get<std::string>();
    e.template_version = j.value("template_version", "");
    e.template_hash = j.at("template_hash").get<std::string>();
    e.evaluator_version = j.value("evaluator_version", "");
    e.target = j.value("target", "");
    e.runner_sha256 = j.value("runner_sha256", "");
    e.timestamp = j.value("timestamp", "");
    e.success = j.value("success", true);
    if (j.contains("sources")) {
      for (const auto& [id, sha] : j.at("sources").items()) {
        e.sources.emplace_back(id, sha.get<std::string>());
      }
    }
    if (j.contains("images")) {
      for (const auto& [img, digest] : j.at("images").items()) {
        e.images.emplace_back(img, digest.get<std::string>());
      }
    }
    if (j.contains("plugin_hashes")) {
      e.plugin_hashes = j.at("plugin_hashes").get<std::vector<std::string>>();
    }
    if (j.contains("artifacts")) {
      for (const auto& [path, sha] : j.at("artifacts").items()) {
        e.artifacts[path] = sha.get<std::string>();
      }
    }
    if (j.contains("symtabs")) {
      for (const auto& [path, sha] : j.at("symtabs").items()) {
        e.symtabs[path] = sha.get<std::string>();
      }
    }
    e.symtab_tool = j.value("symtab_tool", "");
    e.buildenv_ref = j.value("buildenv_ref", "");
    if (j.contains("dep_manifests")) {
      e.dep_manifests = j.at("dep_manifests").get<std::vector<std::string>>();
    }
    if (j.contains("dep_scan_skipped")) {
      e.dep_scan_skipped = j.at("dep_scan_skipped").get<std::vector<std::string>>();
    }
    return e;
  } catch (const std::exception& ex) {
    return tl::unexpected(
        Error::make(ErrorCode::Parse, "账本解析失败: " + std::string(ex.what()), context));
  }
}

Result<void> LedgerStore::append(const std::string& dir, const LedgerEntry& entry) {
  if (auto r = fs::ensure_dir(dir); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  const std::string path = dir + "/" + entry.build_id + ".json";
  if (auto r = fs::atomic_write(path, entry.to_json()); !r) {
    return tl::unexpected(std::move(r.error()));
  }

  // 追加索引（读-改-写原子替换）
  const std::string index_path = dir + "/index.json";
  nlohmann::json index = nlohmann::json::array();
  if (fs::exists(index_path)) {
    if (auto existing = fs::read_file(index_path); existing) {
      try {
        index = nlohmann::json::parse(*existing);
      } catch (const std::exception&) {
        // 索引损坏则重建（账本条目本体仍在）
        index = nlohmann::json::array();
      }
    }
  }
  bool replaced = false;
  for (auto& item : index) {
    if (item.is_string() && item.get<std::string>() == entry.build_id) {
      replaced = true;
      break;
    }
  }
  if (!replaced) {
    index.push_back(entry.build_id);
  }
  if (auto r = fs::atomic_write(index_path, index.dump(2)); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  return {};
}

Result<std::optional<LedgerEntry>> LedgerStore::load(const std::string& dir,
                                                     const std::string& build_id) {
  const std::string path = dir + "/" + build_id + ".json";
  if (!fs::exists(path)) {
    return std::nullopt;
  }
  auto bytes = fs::read_file(path);
  if (!bytes) {
    return tl::unexpected(std::move(bytes.error()));
  }
  auto entry = LedgerEntry::from_json(*bytes, path);
  if (!entry) {
    return tl::unexpected(std::move(entry.error()));
  }
  return std::optional<LedgerEntry>{std::move(*entry)};
}

Result<std::vector<std::string>> LedgerStore::list(const std::string& dir) {
  std::vector<std::string> out;
  const std::string index_path = dir + "/index.json";
  if (!fs::exists(index_path)) {
    return out;  // 空账本
  }
  auto bytes = fs::read_file(index_path);
  if (!bytes) {
    return tl::unexpected(std::move(bytes.error()));
  }
  try {
    const auto index = nlohmann::json::parse(*bytes);
    for (const auto& item : index) {
      if (item.is_string()) {
        out.push_back(item.get<std::string>());
      }
    }
  } catch (const std::exception& e) {
    return tl::unexpected(
        Error::make(ErrorCode::Parse, "账本索引损坏: " + std::string(e.what()), index_path));
  }
  return out;
}

}  // namespace w2ucem

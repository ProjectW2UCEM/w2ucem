#include "w2ucem/repo/repo_index.hpp"

#include <nlohmann/json.hpp>

#include "w2ucem/util/error.hpp"

namespace w2ucem {

Result<RepoIndex> RepoIndex::from_json(const std::string& json, const std::string& context) {
  try {
    const auto j = nlohmann::json::parse(json);
    RepoIndex idx;
    idx.schema_version = j.at("schema_version").get<int>();
    idx.generated_at = j.value("generated_at", "");
    for (const auto& t : j.at("templates")) {
      IndexedTemplate entry;
      entry.id = t.at("id").get<std::string>();
      entry.version = t.at("version").get<std::string>();
      entry.sha256 = t.at("sha256").get<std::string>();
      entry.path = t.at("path").get<std::string>();
      entry.review_status = t.value("review_status", "reviewed");
      if (t.contains("channels")) {
        entry.channels = t.at("channels").get<std::vector<std::string>>();
      }
      idx.templates.push_back(std::move(entry));
    }
    return idx;
  } catch (const std::exception& e) {
    return tl::unexpected(
        Error::make(ErrorCode::Parse, "仓库索引解析失败: " + std::string(e.what()), context));
  }
}

std::string RepoIndex::to_json() const {
  nlohmann::json j;
  j["schema_version"] = schema_version;
  j["generated_at"] = generated_at;
  j["templates"] = nlohmann::json::array();
  for (const auto& t : templates) {
    nlohmann::json tj;
    tj["id"] = t.id;
    tj["version"] = t.version;
    tj["sha256"] = t.sha256;
    tj["path"] = t.path;
    tj["review_status"] = t.review_status;
    tj["channels"] = t.channels;
    j["templates"].push_back(std::move(tj));
  }
  return j.dump(2);
}

const IndexedTemplate* RepoIndex::find(const std::string& id) const {
  for (const auto& t : templates) {
    if (t.id == id) {
      return &t;
    }
  }
  return nullptr;
}

const IndexedTemplate* RepoIndex::find(const std::string& id, const std::string& version) const {
  for (const auto& t : templates) {
    if (t.id == id && t.version == version) {
      return &t;
    }
  }
  return nullptr;
}

}  // namespace w2ucem

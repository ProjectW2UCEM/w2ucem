#include "w2ucem/usertemplate/registry.hpp"

#include <nlohmann/json.hpp>

#include "w2ucem/usertemplate/state_machine.hpp"
#include "w2ucem/usertemplate/sync_client.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"

namespace w2ucem {

namespace {

std::string registry_path(const std::string& dir) {
  return dir + "/registry.json";
}

nlohmann::json meta_to_json(const UserTemplateMeta& meta) {
  nlohmann::json j;
  j["id"] = meta.id;
  j["maintainer"] = {{"name", meta.maintainer.name}, {"email", meta.maintainer.email}};
  j["openpgp_fingerprint"] = meta.openpgp_fingerprint;
  j["ssh_pubkey"] = meta.ssh_pubkey;
  j["last_heartbeat"] = meta.last_heartbeat;
  j["submitted_at"] = meta.submitted_at;
  j["status"] = user_status_str(meta.status);
  j["review_status"] = review_status_str(meta.review_status);
  j["evidences"] = nlohmann::json::array();
  for (const auto& e : meta.evidences) {
    j["evidences"].push_back(
        {{"type", auth_evidence_str(e.type)}, {"detail", e.detail}, {"verified_at", e.verified_at}});
  }
  if (meta.investigation) {
    j["investigation"] = *meta.investigation;
  }
  return j;
}

Result<UserTemplateMeta> meta_from_json(const nlohmann::json& j, const std::string& context) {
  try {
    UserTemplateMeta meta;
    meta.id = j.at("id").get<std::string>();
    meta.maintainer.name = j.at("maintainer").at("name").get<std::string>();
    meta.maintainer.email = j.at("maintainer").at("email").get<std::string>();
    meta.openpgp_fingerprint = j.value("openpgp_fingerprint", "");
    meta.ssh_pubkey = j.value("ssh_pubkey", "");
    meta.last_heartbeat = j.at("last_heartbeat").get<std::string>();
    meta.submitted_at = j.value("submitted_at", "");
    const auto status = user_status_from_str(j.at("status").get<std::string>());
    const auto review = review_status_from_str(j.at("review_status").get<std::string>());
    if (!status || !review) {
      return tl::unexpected(
          Error::make(ErrorCode::Parse, "注册表状态值非法", context));
    }
    meta.status = *status;
    meta.review_status = *review;
    if (j.contains("evidences")) {
      for (const auto& e : j.at("evidences")) {
        AuthEvidence ev;
        const std::string type_str = e.at("type").get<std::string>();
        if (type_str == "browser") ev.type = AuthEvidenceType::Browser;
        else if (type_str == "cookie") ev.type = AuthEvidenceType::Cookie;
        else if (type_str == "openpgp") ev.type = AuthEvidenceType::OpenPgp;
        else if (type_str == "ssh") ev.type = AuthEvidenceType::Ssh;
        else if (type_str == "ip") ev.type = AuthEvidenceType::Ip;
        else continue;
        ev.detail = e.value("detail", "");
        ev.verified_at = e.value("verified_at", "");
        meta.evidences.push_back(std::move(ev));
      }
    }
    if (j.contains("investigation")) {
      meta.investigation = j.at("investigation").get<std::string>();
    }
    return meta;
  } catch (const std::exception& ex) {
    return tl::unexpected(
        Error::make(ErrorCode::Parse, "注册表条目解析失败: " + std::string(ex.what()), context));
  }
}

}  // namespace

Result<UserTemplateRegistry> UserTemplateRegistry::load(const std::string& dir) {
  UserTemplateRegistry registry;
  registry.dir_ = dir;
  const std::string path = registry_path(dir);
  if (!fs::exists(path)) {
    return registry;  // 空注册表
  }
  auto bytes = fs::read_file(path);
  if (!bytes) {
    return tl::unexpected(std::move(bytes.error()));
  }
  try {
    const auto j = nlohmann::json::parse(*bytes);
    if (j.contains("templates")) {
      for (const auto& item : j.at("templates")) {
        auto meta = meta_from_json(item, path);
        if (!meta) {
          return tl::unexpected(std::move(meta.error()));
        }
        registry.entries_.push_back(std::move(*meta));
      }
    }
  } catch (const std::exception& ex) {
    return tl::unexpected(
        Error::make(ErrorCode::Parse, "注册表解析失败: " + std::string(ex.what()), path));
  }
  return registry;
}

Result<void> UserTemplateRegistry::save() const {
  nlohmann::json j;
  j["templates"] = nlohmann::json::array();
  for (const auto& meta : entries_) {
    j["templates"].push_back(meta_to_json(meta));
  }
  if (auto r = fs::ensure_dir(dir_); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  return fs::atomic_write(registry_path(dir_), j.dump(2));
}

Result<void> UserTemplateRegistry::add(const UserTemplateMeta& meta) {
  for (const auto& existing : entries_) {
    if (existing.id == meta.id) {
      return tl::unexpected(Error::make(ErrorCode::EvaluationError, "用户模板 id 已存在", meta.id));
    }
  }
  entries_.push_back(meta);
  return save();
}

Result<UserTemplateMeta> UserTemplateRegistry::heartbeat(const std::string& id,
                                                         const std::string& now_iso) {
  for (auto& meta : entries_) {
    if (meta.id != id) {
      continue;
    }
    // 状态机先于心跳更新执行（超期封存不受本次心跳影响）
    const auto action = UserTemplateStateMachine::advance(meta, now_iso);
    meta.last_heartbeat = now_iso;
    if (auto r = UserTemplateStateMachine::apply(meta, action); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    if (auto r = save(); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    return meta;
  }
  return tl::unexpected(Error::make(ErrorCode::EvaluationError, "用户模板不存在", id));
}

std::vector<UserTemplateMeta> UserTemplateRegistry::list(bool include_archived) const {
  std::vector<UserTemplateMeta> out;
  for (const auto& meta : entries_) {
    if (!include_archived && meta.status == UserStatus::Archived) {
      continue;  // "人死删包"：封存条目默认隐藏
    }
    out.push_back(meta);
  }
  return out;
}

std::optional<UserTemplateMeta> UserTemplateRegistry::find(const std::string& id) const {
  for (const auto& meta : entries_) {
    if (meta.id == id) {
      return meta;
    }
  }
  return std::nullopt;
}

// ---------- StubSyncClient ----------

Result<void> StubSyncClient::push_heartbeat(const std::string& id, const std::string& now_iso) {
  (void)id;
  (void)now_iso;
  return tl::unexpected(Error::make(ErrorCode::Unsupported,
                                    "服务端心跳 API 未实现（alpha），见 docs/REPO-PROTOCOL.md"));
}

Result<std::vector<UserTemplateMeta>> StubSyncClient::fetch_pending_review() {
  return tl::unexpected(Error::make(ErrorCode::Unsupported,
                                    "服务端审核 API 未实现（alpha），见 docs/REPO-PROTOCOL.md"));
}

}  // namespace w2ucem

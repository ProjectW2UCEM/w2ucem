// 用户模板元数据：AUR 式社区体系的本地数据模型。
// 心跳/审核/身份验证字段为 alpha 本地落地；服务端协议见 docs/REPO-PROTOCOL.md。
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace w2ucem {

enum class UserStatus {
  Active,    // 活跃
  HalfDead,  // 半死：心跳超时，包仍可用但标记
  Archived,  // 封存（"人死删包"）：registry 隐藏 + 文件不删除
};

enum class ReviewStatus {
  Experimental,  // 实验性：官方未审核（AUR 教训：默认不信任）
  Reviewed,      // 官方审核通过
};

enum class AuthEvidenceType {
  Browser,   // 浏览器会话证据
  Cookie,    // Cookie 证据
  OpenPgp,   // OpenPGP 签名
  Ssh,       // SSH 公钥
  Ip,        // IP 地址一致性
};

struct AuthEvidence {
  AuthEvidenceType type;
  std::string detail;
  std::string verified_at;  // ISO8601
};

struct Maintainer {
  std::string name;
  std::string email;
};

struct UserTemplateMeta {
  std::string id;
  Maintainer maintainer;
  std::string openpgp_fingerprint;
  std::string ssh_pubkey;
  std::vector<AuthEvidence> evidences;
  std::string last_heartbeat;  // ISO8601
  std::string submitted_at;    // ISO8601
  UserStatus status{UserStatus::Active};
  ReviewStatus review_status{ReviewStatus::Experimental};
  std::optional<std::string> investigation;  // 官方调查备注
};

[[nodiscard]] const char* user_status_str(UserStatus s);
[[nodiscard]] const char* review_status_str(ReviewStatus s);
[[nodiscard]] const char* auth_evidence_str(AuthEvidenceType t);
[[nodiscard]] std::optional<UserStatus> user_status_from_str(const std::string& s);
[[nodiscard]] std::optional<ReviewStatus> review_status_from_str(const std::string& s);

}  // namespace w2ucem

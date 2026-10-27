#include "w2ucem/usertemplate/meta.hpp"

namespace w2ucem {

const char* user_status_str(UserStatus s) {
  switch (s) {
    case UserStatus::Active: return "active";
    case UserStatus::HalfDead: return "half-dead";
    case UserStatus::Archived: return "archived";
  }
  return "?";
}

const char* review_status_str(ReviewStatus s) {
  switch (s) {
    case ReviewStatus::Experimental: return "experimental";
    case ReviewStatus::Reviewed: return "reviewed";
  }
  return "?";
}

const char* auth_evidence_str(AuthEvidenceType t) {
  switch (t) {
    case AuthEvidenceType::Browser: return "browser";
    case AuthEvidenceType::Cookie: return "cookie";
    case AuthEvidenceType::OpenPgp: return "openpgp";
    case AuthEvidenceType::Ssh: return "ssh";
    case AuthEvidenceType::Ip: return "ip";
  }
  return "?";
}

std::optional<UserStatus> user_status_from_str(const std::string& s) {
  if (s == "active") return UserStatus::Active;
  if (s == "half-dead") return UserStatus::HalfDead;
  if (s == "archived") return UserStatus::Archived;
  return std::nullopt;
}

std::optional<ReviewStatus> review_status_from_str(const std::string& s) {
  if (s == "experimental") return ReviewStatus::Experimental;
  if (s == "reviewed") return ReviewStatus::Reviewed;
  return std::nullopt;
}

}  // namespace w2ucem

#include "w2ucem/usertemplate/state_machine.hpp"

#include <ctime>

#include "w2ucem/util/error.hpp"

namespace w2ucem {

namespace {

// 解析 %Y-%m-%dT%H:%M:%SZ → time_t；失败返回 -1。
std::time_t parse_iso8601_utc(const std::string& s) {
  std::tm tm{};
  // 手工解析：YYYY-MM-DDTHH:MM:SSZ
  if (s.size() != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
      s[16] != ':' || s[19] != 'Z') {
    return -1;
  }
  const auto num = [&s](std::size_t pos, std::size_t len) -> int {
    int v = 0;
    for (std::size_t i = 0; i < len; ++i) {
      const char c = s[pos + i];
      if (c < '0' || c > '9') {
        return -1;
      }
      v = v * 10 + (c - '0');
    }
    return v;
  };
  const int year = num(0, 4);
  const int month = num(5, 2);
  const int day = num(8, 2);
  const int hour = num(11, 2);
  const int minute = num(14, 2);
  const int second = num(17, 2);
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
      minute > 59 || second > 60) {
    return -1;
  }
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
#if defined(_WIN32)
  return _mkgmtime(&tm);
#else
  return timegm(&tm);
#endif
}

}  // namespace

std::optional<double> UserTemplateStateMachine::days_between(const std::string& from_iso,
                                                             const std::string& to_iso) {
  const auto from = parse_iso8601_utc(from_iso);
  const auto to = parse_iso8601_utc(to_iso);
  if (from < 0 || to < 0) {
    return std::nullopt;
  }
  return std::difftime(to, from) / 86400.0;
}

UserTemplateStateMachine::Action UserTemplateStateMachine::advance(
    const UserTemplateMeta& meta, const std::string& now_iso) {
  const auto age = days_between(meta.last_heartbeat, now_iso);
  if (!age || *age < 0) {
    return Action::None;  // 心跳时间无效或未来时间：不动
  }
  switch (meta.status) {
    case UserStatus::Active:
      if (*age > kHeartbeatMaxAgeDays) {
        return Action::MarkHalfDead;
      }
      return Action::None;
    case UserStatus::HalfDead:
      if (*age <= kHeartbeatMaxAgeDays) {
        return Action::Reactivate;
      }
      if (*age > kOrphanThresholdDays) {
        return Action::Archive;
      }
      return Action::None;
    case UserStatus::Archived:
      return Action::None;  // 申诉恢复为人工流程（alpha 仅接口）
  }
  return Action::None;
}

Result<void> UserTemplateStateMachine::apply(UserTemplateMeta& meta, Action action) {
  switch (action) {
    case Action::None:
      return {};
    case Action::MarkHalfDead:
      if (meta.status != UserStatus::Active) {
        return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                          "非法状态转移：仅 Active 可标记半死", meta.id));
      }
      meta.status = UserStatus::HalfDead;
      return {};
    case Action::Reactivate:
      if (meta.status != UserStatus::HalfDead) {
        return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                          "非法状态转移：仅 HalfDead 可恢复", meta.id));
      }
      meta.status = UserStatus::Active;
      return {};
    case Action::Archive:
      if (meta.status != UserStatus::HalfDead) {
        return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                          "非法状态转移：仅 HalfDead 可封存", meta.id));
      }
      meta.status = UserStatus::Archived;
      return {};
  }
  return tl::unexpected(Error::make(ErrorCode::EvaluationError, "未知状态机动作", meta.id));
}

}  // namespace w2ucem

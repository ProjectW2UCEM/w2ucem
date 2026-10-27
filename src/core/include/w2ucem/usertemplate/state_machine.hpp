// 用户模板状态机（纯函数）：
//   Active --心跳超 7 天--> HalfDead --心跳恢复--> Active
//   HalfDead --超孤儿阈值(30 天)--> Archived（"人死删包"=隐藏+封存，文件不删除）
//   Archived --申诉（人工）--> Active（alpha 仅接口）
// 审核：Experimental <-> Reviewed（官方操作）
#pragma once

#include <string>

#include "w2ucem/usertemplate/meta.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

// 心跳阈值（规格：每周必须在线签到一次）
inline constexpr int kHeartbeatMaxAgeDays = 7;
// 孤儿判定阈值：半死后超此时长无人认领 → 封存
inline constexpr int kOrphanThresholdDays = 30;

struct UserTemplateStateMachine {
  enum class Action {
    None,
    MarkHalfDead,  // Active → HalfDead
    Reactivate,    // HalfDead → Active
    Archive,       // HalfDead → Archived
  };

  // 纯函数：根据心跳时间与当前状态推导动作。now_iso 为 ISO8601 UTC 时间。
  [[nodiscard]] static Action advance(const UserTemplateMeta& meta, const std::string& now_iso);

  // 应用动作（非法转移返回 EvaluationError）。
  [[nodiscard]] static Result<void> apply(UserTemplateMeta& meta, Action action);

  // ISO8601 UTC（%Y-%m-%dT%H:%M:%SZ）解析与天数差；解析失败返回 nullopt。
  [[nodiscard]] static std::optional<double> days_between(const std::string& from_iso,
                                                         const std::string& to_iso);
};

}  // namespace w2ucem

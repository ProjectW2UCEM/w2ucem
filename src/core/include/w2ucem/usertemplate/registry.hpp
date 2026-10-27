// 本地用户模板注册表：CRUD + 心跳更新（状态机驱动）。
// Archived 条目在 list 默认隐藏（"人死删包"= 隐藏封存，不删除文件）。
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "w2ucem/usertemplate/meta.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

class UserTemplateRegistry {
 public:
  // dir 不存在时创建空注册表。
  [[nodiscard]] static Result<UserTemplateRegistry> load(const std::string& dir);

  [[nodiscard]] Result<void> save() const;

  [[nodiscard]] Result<void> add(const UserTemplateMeta& meta);

  // 更新心跳并驱动状态机（超期封存/恢复）。
  [[nodiscard]] Result<UserTemplateMeta> heartbeat(const std::string& id,
                                                   const std::string& now_iso);

  // include_archived=false 时隐藏封存条目。
  [[nodiscard]] std::vector<UserTemplateMeta> list(bool include_archived = false) const;

  [[nodiscard]] std::optional<UserTemplateMeta> find(const std::string& id) const;

 private:
  std::string dir_;
  std::vector<UserTemplateMeta> entries_;
};

}  // namespace w2ucem

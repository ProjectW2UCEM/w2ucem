// 插件 hook 点：模板内声明式 hook，插件受哈希校验入账本。
// alpha 实现 PreAssemble/PostAssemble，其余枚举就位。
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace w2ucem {

enum class HookPoint {
  PreFetch,
  PostFetch,
  PreAssemble,
  PostAssemble,
  PreBuild,
  PostBuild,
  PostInstall,
};

[[nodiscard]] const char* hook_point_str(HookPoint point);
// 模板 TOML 键名（"pre-fetch" 等）→ HookPoint；非法返回 nullopt。
[[nodiscard]] std::optional<HookPoint> hook_point_from_str(const std::string& s);

struct Hooks {
  std::map<HookPoint, std::vector<std::string>> plugins;
};

}  // namespace w2ucem

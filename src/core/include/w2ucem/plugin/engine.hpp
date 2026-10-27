// PluginEngine：hook 调度。
// alpha 实现 PreAssemble/PostAssemble（exec-in-container：payload 注入 runner 脚本），
// 返回脚本补丁与插件哈希账（由 orchestrator 记入 ledger）。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/plugin/plugin.hpp"
#include "w2ucem/template/hooks.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct PluginScriptPatch {
  std::string script_fragment;                          // 注入 runner 脚本的片段
  std::vector<std::pair<std::string, std::string>> plugin_hashes;  // id → payload sha256
};

struct PluginEngine {
  // plugins_root：<root>/lib/plugins/<id>/plugin.toml
  // point 无声明插件时返回空补丁（不报错）。
  [[nodiscard]] static Result<PluginScriptPatch> run_hooks(
      HookPoint point, const std::vector<std::string>& plugin_ids,
      const std::string& plugins_root, bool allow_host_script);
};

}  // namespace w2ucem

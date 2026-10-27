// 插件：模板内声明式 hook 的载荷。
// 可复现性约束：插件描述 + payload 全部 sha256 入账本，
// payload 执行发生在容器内（exec-in-container），输入输出声明进账本。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/template/hooks.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

enum class PluginPayloadKind {
  ExecInContainer,  // 容器内执行（注入 runner 脚本）
  HostScript,       // 宿主脚本（alpha 后置，安全审核后启用）
};

struct PluginSpec {
  std::string id;
  std::string version;
  std::string sha256;      // payload 脚本内容哈希（manifest 声明）
  HookPoint hook;          // 支持的 hook 点（alpha 单一）
  PluginPayloadKind payload_kind;
  std::string payload;     // 脚本内容
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
};

struct PluginManifestParser {
  // plugin.toml 解析（严格 schema）：id/version/hook/payload 必填。
  [[nodiscard]] static Result<PluginSpec> parse(const std::string& toml_bytes,
                                                const std::string& context);
};

}  // namespace w2ucem

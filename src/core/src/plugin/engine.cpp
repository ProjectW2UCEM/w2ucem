#include "w2ucem/plugin/engine.hpp"

#include "w2ucem/plugin/plugin.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/logging.hpp"

namespace w2ucem {

Result<PluginScriptPatch> PluginEngine::run_hooks(HookPoint point,
                                                  const std::vector<std::string>& plugin_ids,
                                                  const std::string& plugins_root,
                                                  bool allow_host_script) {
  PluginScriptPatch patch;
  if (plugin_ids.empty()) {
    return patch;
  }

  for (const auto& id : plugin_ids) {
    const std::string manifest_path = plugins_root + "/" + id + "/plugin.toml";
    auto bytes = fs::read_file(manifest_path);
    if (!bytes) {
      return tl::unexpected(Error::make(ErrorCode::Io,
                                        "插件 manifest 不存在（模板 hook 引用了未安装的插件）",
                                        manifest_path));
    }
    auto spec = PluginManifestParser::parse(*bytes, "plugin:" + id);
    if (!spec) {
      return tl::unexpected(std::move(spec.error()));
    }
    if (spec->hook != point) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "插件 " + id + " 声明 hook=" + hook_point_str(spec->hook) +
                                            "，模板要求 " + hook_point_str(point),
                                        id));
    }
    if (spec->payload_kind == PluginPayloadKind::HostScript && !allow_host_script) {
      return tl::unexpected(Error::make(ErrorCode::Unsupported,
                                        "host-script 插件为 alpha 后功能（安全审核后启用）", id));
    }

    patch.plugin_hashes.emplace_back(id, spec->sha256);
    // exec-in-container：payload 注入 runner 脚本（在容器内执行）
    patch.script_fragment += "\n# ---- plugin: " + id + " (sha256 " + spec->sha256 + ") ----\n";
    patch.script_fragment += spec->payload;
    patch.script_fragment += "\n# ---- end plugin: " + id + " ----\n\n";

    Logger::instance().info("插件 {} 挂载于 {}（payload sha256 {}）", id, hook_point_str(point),
                            spec->sha256.substr(0, 12));
  }
  return patch;
}

}  // namespace w2ucem

#include "w2ucem/plugin/plugin.hpp"

#include <toml.hpp>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/sha256.hpp"

namespace w2ucem {

Result<PluginSpec> PluginManifestParser::parse(const std::string& toml_bytes,
                                               const std::string& context) {
  toml::value root;
  try {
    root = toml::parse_str(toml_bytes);
  } catch (const toml::syntax_error& e) {
    return tl::unexpected(Error::make(ErrorCode::Parse, e.what(), context));
  }

  // 严格 schema：未知键拒绝
  const auto allowed = [&](const toml::value& t, std::initializer_list<const char*> keys,
                           const std::string& path) -> Result<void> {
    for (const auto& [k, v] : t.as_table()) {
      (void)v;
      bool found = false;
      for (const char* a : keys) {
        if (k == a) {
          found = true;
          break;
        }
      }
      if (!found) {
        return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                          "未知键 '" + k + "'（位置: " + path + "）", context));
      }
    }
    return {};
  };
  if (auto r = allowed(root, {"plugin"}, "<root>"); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (!root.contains("plugin")) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation, "缺少 [plugin] 表", context));
  }
  const auto& p = root.at("plugin");
  if (auto r = allowed(p, {"id", "version", "hook", "payload_kind", "payload", "inputs", "outputs"},
                       "plugin");
      !r) {
    return tl::unexpected(std::move(r.error()));
  }

  try {
    PluginSpec spec;
    spec.id = toml::find<std::string>(p, "id");
    spec.version = toml::find<std::string>(p, "version");
    spec.payload = toml::find<std::string>(p, "payload");

    const std::string hook_str = toml::find<std::string>(p, "hook");
    const auto hook = hook_point_from_str(hook_str);
    if (!hook) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "未知 hook 点 '" + hook_str + "'", context));
    }
    spec.hook = *hook;

    spec.payload_kind = PluginPayloadKind::ExecInContainer;
    if (p.contains("payload_kind")) {
      const std::string kind = toml::find<std::string>(p, "payload_kind");
      if (kind == "exec-in-container") {
        spec.payload_kind = PluginPayloadKind::ExecInContainer;
      } else if (kind == "host-script") {
        spec.payload_kind = PluginPayloadKind::HostScript;
      } else {
        return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                          "未知 payload_kind '" + kind + "'", context));
      }
    }
    if (p.contains("inputs")) {
      spec.inputs = toml::find<std::vector<std::string>>(p, "inputs");
    }
    if (p.contains("outputs")) {
      spec.outputs = toml::find<std::vector<std::string>>(p, "outputs");
    }

    spec.sha256 = Sha256::hex(Sha256::of_bytes(spec.payload));
    return spec;
  } catch (const std::exception& e) {
    return tl::unexpected(
        Error::make(ErrorCode::SchemaViolation, "插件 manifest 字段错误: " + std::string(e.what()),
                    context));
  }
}

}  // namespace w2ucem

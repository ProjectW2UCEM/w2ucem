#include "w2ucem/config/config.hpp"

#include <toml.hpp>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"

namespace w2ucem {

Config ConfigLoader::defaults(const std::string& root_override) {
  Config config;
  // root_override 为空时 DataDirs::create 取平台默认；create 失败在 load 层处理，
  // defaults 用于测试路径（root 必给）。
  const auto dirs = DataDirs::create(root_override);
  if (dirs) {
    config.dirs = *dirs;
  }
  return config;
}

Result<Config> ConfigLoader::load(const std::string& config_path,
                                  const std::string& root_override) {
  Config config = defaults(root_override);
  const auto apply_file = [&config](const std::string& path) -> Result<void> {
    if (path.empty() || !fs::exists(path)) {
      return {};
    }
    toml::value root;
    try {
      root = toml::parse(path);
    } catch (const toml::syntax_error& e) {
      return tl::unexpected(Error::make(ErrorCode::Parse, e.what(), path));
    }
    const auto table = root.as_table();
    if (table.count("w2ucem") == 0) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "配置文件缺少 [w2ucem] 表", path));
    }
    const auto cfg = toml::find(root, "w2ucem");
    try {
      if (cfg.contains("repo_urls")) {
        config.repo_urls = toml::find<std::vector<std::string>>(cfg, "repo_urls");
      }
      if (cfg.contains("archive_urls")) {
        config.archive_urls = toml::find<std::vector<std::string>>(cfg, "archive_urls");
      }
      if (cfg.contains("container_engine")) {
        config.container_engine = toml::find<std::string>(cfg, "container_engine");
      }
      if (cfg.contains("sig_pubkey_file")) {
        config.sig_pubkey_file = toml::find<std::string>(cfg, "sig_pubkey_file");
      }
      if (cfg.contains("allow_unsigned_index")) {
        config.allow_unsigned_index = toml::find<bool>(cfg, "allow_unsigned_index");
      }
    } catch (const std::exception& e) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "配置字段类型错误: " + std::string(e.what()), path));
    }
    return {};
  };

  // 1. 用户显式配置文件（--config）
  if (auto r = apply_file(config_path); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  // 2. 本地持久化覆盖（sync --repo 写入，优先级最高）
  const std::string local_cfg = config.dirs.root() + "/config.toml";
  if (auto r = apply_file(local_cfg); !r) {
    return tl::unexpected(std::move(r.error()));
  }

  return config;
}

}  // namespace w2ucem

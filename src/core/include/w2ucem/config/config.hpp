// Config：仓库/Archive URL、容器引擎、签名信任配置。
// 配置文件（TOML）可选：不存在时使用默认值（默认仓库 URL 为占位，
// alpha 的实际仓库以 file:// 测试仓库扮演）。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/config/data_dirs.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct Config {
  DataDirs dirs;

  std::vector<std::string> repo_urls{"https://repo.w2ucem.org"};
  std::vector<std::string> archive_urls{"https://archive.w2ucem.org"};

  // auto | docker | podman | fake
  std::string container_engine{"auto"};

  // 空 = 使用默认信任位置 <dirs>/trust/pubkey.pem
  std::string sig_pubkey_file;

  // 仅测试用；产品语义默认拒绝未签名索引。
  bool allow_unsigned_index{false};
};

struct ConfigLoader {
  // config_path 为空时：不读配置文件，直接返回 defaults。
  // 文件存在则解析 TOML（[w2ucem] 表），未知键报 SchemaViolation。
  [[nodiscard]] static Result<Config> load(const std::string& config_path,
                                           const std::string& root_override);

  [[nodiscard]] static Config defaults(const std::string& root_override);
};

}  // namespace w2ucem

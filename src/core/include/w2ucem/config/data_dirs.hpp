// DataDirs：XDG 风格数据目录布局。
// --root 可整体重定向（测试/隔离，pacman -r 风格）。
// 布局：
//   <root>/sync/repo.db           仓库索引 + 签名（原子替换）
//   <root>/cache/sources/         已校验源文件缓存（按 sha256 命名）
//   <root>/lib/templates/         已下载的模板原始字节
//   <root>/archive/blob/          时光机内容寻址缓存（sha256:<hex>）
//   <root>/runtime/<t>/<channel>/<family>/<gpu>/   产物树
//   <root>/runtime/<t>/<channel>/<family>/<gpu>/ledger/  可复现性账本
#pragma once

#include <string>

#include "w2ucem/channel/target.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

class DataDirs {
 public:
  // root_override 为空时取平台默认：
  //   Linux: $XDG_DATA_HOME/w2ucem 或 ~/.local/share/w2ucem
  //   macOS: ~/Library/Application Support/w2ucem
  [[nodiscard]] static Result<DataDirs> create(const std::string& root_override);

  [[nodiscard]] const std::string& root() const { return root_; }

  [[nodiscard]] std::string sync_dir() const { return root_ + "/sync"; }
  [[nodiscard]] std::string index_path() const { return sync_dir() + "/repo.db"; }
  [[nodiscard]] std::string index_sig_path() const { return sync_dir() + "/repo.db.sig"; }

  [[nodiscard]] std::string sources_cache() const { return root_ + "/cache/sources"; }
  [[nodiscard]] std::string templates_dir() const { return root_ + "/lib/templates"; }
  [[nodiscard]] std::string archive_blob_dir() const { return root_ + "/archive/blob"; }
  [[nodiscard]] std::string plugins_dir() const { return root_ + "/lib/plugins"; }
  [[nodiscard]] std::string user_templates_dir() const { return root_ + "/lib/user-templates"; }
  [[nodiscard]] std::string trust_dir() const { return root_ + "/trust"; }
  [[nodiscard]] std::string default_pubkey_path() const { return trust_dir() + "/pubkey.pem"; }

  // 产物根：runtime/<template>/<channel>/<family>/<gpu>/
  [[nodiscard]] std::string runtime_dir(const std::string& template_id,
                                        const BuildTarget& target) const;
  // 账本目录：产物根下的 ledger/
  [[nodiscard]] std::string ledger_dir(const std::string& template_id,
                                       const BuildTarget& target) const;

 private:
  std::string root_;
};

}  // namespace w2ucem

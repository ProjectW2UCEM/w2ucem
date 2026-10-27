// 仓库索引：中央仓库的模板目录（带哈希与审核状态）。
// 索引由维护者签名发布（ed25519），客户端 sync 时验签。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct IndexedTemplate {
  std::string id;
  std::string version;
  std::string sha256;   // 模板原始字节哈希（fetch 后校验）
  std::string path;     // 仓库内相对路径（如 templates/proton-ge/9.27/template.toml）
  std::string review_status;  // "experimental" | "reviewed"
  std::vector<std::string> channels;
};

struct RepoIndex {
  int schema_version{1};
  std::string generated_at;
  std::vector<IndexedTemplate> templates;

  [[nodiscard]] static Result<RepoIndex> from_json(const std::string& json,
                                                   const std::string& context);
  [[nodiscard]] std::string to_json() const;

  [[nodiscard]] const IndexedTemplate* find(const std::string& id) const;
  [[nodiscard]] const IndexedTemplate* find(const std::string& id,
                                            const std::string& version) const;
};

}  // namespace w2ucem

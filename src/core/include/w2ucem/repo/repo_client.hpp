// RepoClient：中央仓库客户端。
// sync：拉取 <url>/repo.db + repo.db.sig → ed25519 验签 → 原子写本地缓存。
// fetch_template：拉取模板原始字节 → sha256 比对索引。
// 多 URL 轮询：任一成功即可。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/repo/repo_index.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct RepoClient {
  // sig_pubkey_path：ed25519 公钥 PEM。allow_unsigned 仅测试用。
  [[nodiscard]] static Result<RepoIndex> sync(const std::vector<std::string>& repo_urls,
                                              const std::string& sig_pubkey_path,
                                              bool allow_unsigned,
                                              const std::string& local_index_dir);

  // 返回模板原始字节（已通过 sha256 校验）。
  [[nodiscard]] static Result<std::string> fetch_template(
      const IndexedTemplate& entry, const std::vector<std::string>& repo_urls);

  // 读本地缓存索引。
  [[nodiscard]] static Result<RepoIndex> read_local_index(const std::string& local_index_dir);
};

}  // namespace w2ucem

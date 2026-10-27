// ArchiveClient：时光机仓库客户端。
// 内容寻址存储：archive_id = "sha256:<64hex>"，本地 blob 缓存按 hex 命名。
// 官方源失效后由 FallbackSourceResolver 自动回退到此处下载。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct ArchiveClient {
  // 从时光机仓库拉取 blob 到本地缓存并校验，返回缓存路径。
  // 本地缓存命中（此前已校验落盘）直接返回。
  [[nodiscard]] static Result<std::string> fetch_to_cache(
      const std::string& archive_id, const std::vector<std::string>& archive_urls,
      const std::string& blob_cache_dir);

  // 客户端侧 ingest：把文件按内容寻址存入本地 blob 缓存，返回 archive_id。
  [[nodiscard]] static Result<std::string> add(const std::string& file_path,
                                               const std::string& blob_cache_dir);
};

}  // namespace w2ucem

// FallbackSourceResolver：源获取的完整回退语义——
// 官方 urls 逐个试（下载 → sha256 校验）→ 全部失败 → archive_id 走时光机 →
// 成功回写内容寻址缓存。这就是"官方源失效后切换时光机"的 alpha 完整落地。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/template/build_plan.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct FallbackSourceResolver {
  // cache_dir：已校验源缓存（按 sha256 命名）；blob_dir：Archive blob 缓存。
  // force_archive：跳过官方 urls 直接走时光机（archive-check 诊断命令用）。
  // 返回：已校验源文件的本地路径。
  [[nodiscard]] static Result<std::string> resolve(const ResolvedSource& source,
                                                   const std::vector<std::string>& archive_urls,
                                                   const std::string& cache_dir,
                                                   const std::string& blob_dir,
                                                   bool force_archive);
};

}  // namespace w2ucem

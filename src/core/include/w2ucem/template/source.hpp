// 源与补丁规格：可复现性校验链的第一环（sha256 必填，alpha 无哈希即拒绝）。
#pragma once

#include <string>
#include <vector>

namespace w2ucem {

struct SourceSpec {
  std::string id;
  std::vector<std::string> urls;  // 有序镜像列表，逐个尝试
  std::string sha256;             // 必填：下载后立即校验
  std::string archive_id;         // 可选：时光机内容寻址 id（sha256:<hex>）
};

struct PatchSpec {
  std::string path;    // 相对模板目录的补丁文件路径
  std::string sha256;  // 必填
};

}  // namespace w2ucem

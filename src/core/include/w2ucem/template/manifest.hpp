// 产物 manifest：校验基准。
// 双模式：pin（sha256 声明值）与 record（空哈希，账本首建记录、复建比对）。
// stable/major 通道禁 record 模式（validator 拒绝）。
#pragma once

#include <string>
#include <vector>

namespace w2ucem {

struct ManifestFileSpec {
  std::string path;    // 相对产物根
  std::string sha256;  // 空 = record 模式
  std::string symtab;  // 预留：符号表快照路径（alpha 不启用）
};

struct ManifestSpec {
  std::vector<ManifestFileSpec> files;

  [[nodiscard]] bool has_record_mode() const;
};

}  // namespace w2ucem

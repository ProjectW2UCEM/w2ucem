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
  // 符号表校验作用域选择器：""（不校验）| "dyn"（仅 .dynsym 动态符号表）
  // | "full"（.dynsym + .symtab）。快照位置是 w2ucem 的内部约定
  // （产物树 .w2ucem/symtab/），模板不声明存储路径。
  std::string symtab;
};

struct ManifestSpec {
  std::vector<ManifestFileSpec> files;

  [[nodiscard]] bool has_record_mode() const;
};

}  // namespace w2ucem

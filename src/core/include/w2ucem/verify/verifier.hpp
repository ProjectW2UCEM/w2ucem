// Verifier：产物校验链。
// pin 模式（manifest 声明 sha256）与 record 模式（账本首建记录/复建比对）。
// 符号表校验（symtab 作用域选择器）见 verify/symtab.hpp。
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "w2ucem/build/ledger.hpp"
#include "w2ucem/template/manifest.hpp"

namespace w2ucem {

namespace verify {
class SymtabExtractor;
}

struct VerificationReport {
  struct Check {
    std::string name;
    bool ok{false};
    std::string detail;
  };
  std::vector<Check> checks;

  [[nodiscard]] bool all_ok() const;
};

struct Verifier {
  // 产物 manifest 校验（存在性 + pin/record sha256）。ledger 为空时 record 条目按"首建"处理。
  [[nodiscard]] static VerificationReport verify_artifacts(
      const std::string& output_dir, const ManifestSpec& manifest,
      const std::optional<LedgerEntry>& ledger);

  // dep-manifest schema 校验（存在性 + 可解析 + schema 版本）。
  [[nodiscard]] static VerificationReport verify_dep_manifests(
      const std::string& output_dir, const std::vector<std::string>& relative_paths);

  // 符号表复验：对 manifest 中声明 symtab 的条目重新提取 + 规范化 + 与账本比对。
  //   ledger 无该条目记录      → ok=true "跳过（账本无记录）"
  //   ledger 为空（首建）      → ok=true "首建，无账本可比对"
  //   extractor 不可用且有记录  → ok=false "无法复验（无容器引擎/宿主无 readelf）"
  //   extractor 为空且无记录    → ok=true "跳过"
  [[nodiscard]] static VerificationReport verify_symtabs(
      const std::string& output_dir, const ManifestSpec& manifest,
      const std::optional<LedgerEntry>& ledger, verify::SymtabExtractor* extractor);
};

}  // namespace w2ucem

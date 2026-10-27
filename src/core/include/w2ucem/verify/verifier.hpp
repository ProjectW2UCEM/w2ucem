// Verifier：产物校验链。
// pin 模式（manifest 声明 sha256）与 record 模式（账本首建记录/复建比对）。
// SymtabChecker 为 alpha 后置接口（符号表快照/比对）。
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "w2ucem/build/ledger.hpp"
#include "w2ucem/template/manifest.hpp"

namespace w2ucem {

struct VerificationReport {
  struct Check {
    std::string name;
    bool ok{false};
    std::string detail;
  };
  std::vector<Check> checks;

  [[nodiscard]] bool all_ok() const;
};

// alpha 接口占位：真实现（readelf 符号表快照）为 alpha 后功能。
class SymtabChecker {
 public:
  virtual ~SymtabChecker() = default;
  // 返回符号表快照文本；alpha 未启用，返回空串。
  [[nodiscard]] virtual std::string snapshot(const std::string& elf_path);
};

struct Verifier {
  // 产物 manifest 校验（存在性 + pin/record sha256）。ledger 为空时 record 条目按"首建"处理。
  [[nodiscard]] static VerificationReport verify_artifacts(
      const std::string& output_dir, const ManifestSpec& manifest,
      const std::optional<LedgerEntry>& ledger);

  // dep-manifest schema 校验（存在性 + 可解析 + schema 版本）。
  [[nodiscard]] static VerificationReport verify_dep_manifests(
      const std::string& output_dir, const std::vector<std::string>& relative_paths);
};

}  // namespace w2ucem

// BuildPlan：求值产物——完全解析、无歧义、可 JSON dump（调试/账本）。
// 同 (template 原始字节, BuildTarget, evaluator_version) 必然产出同一 BuildPlan。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/channel/target.hpp"
#include "w2ucem/template/assemble.hpp"
#include "w2ucem/template/manifest.hpp"

namespace w2ucem {

struct ResolvedSource {
  std::string id;
  std::vector<std::string> urls;
  std::string sha256;
  std::string archive_id;
};

struct BuildPlan {
  BuildTarget target;
  std::string template_id;
  std::string template_version;
  std::string template_hash;  // 模板原始字节 sha256
  std::vector<ResolvedSource> sources;
  std::vector<std::string> build_dep_packages;  // 已按 target family 解析
  std::vector<AssembleStep> steps;
  ManifestSpec manifest;
  std::string evaluator_version;

  [[nodiscard]] std::string to_json() const;
};

}  // namespace w2ucem

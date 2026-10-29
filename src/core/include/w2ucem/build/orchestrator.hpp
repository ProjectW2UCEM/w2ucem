// BuildOrchestrator：构建流水线编排——
//   evaluate → fetch sources（镜像+Archive 回退）→ pull buildenv（digest 锁）
//   → 生成 runner（确定性）→ 容器装配 → 矩阵容器 dep-scan
//   → Verifier → Ledger 落账本。
// 插件 hook（PreAssemble/PostAssemble）为 P5 接入点。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/build/backend.hpp"
#include "w2ucem/build/ledger.hpp"
#include "w2ucem/channel/target.hpp"
#include "w2ucem/config/config.hpp"
#include "w2ucem/template/template.hpp"
#include "w2ucem/util/result.hpp"
#include "w2ucem/verify/verifier.hpp"

namespace w2ucem {

struct BuildRequest {
  Template tpl;                 // 已解析+校验的模板
  std::string template_hash;    // 模板原始字节 sha256
  BuildTarget target;
  std::string engine{"auto"};   // auto|docker|podman|fake
  bool full_matrix{false};      // true: 全部矩阵 cell 跑 dep-scan
  bool keep_container{false};
  bool verify_on{true};
};

struct BuildResult {
  std::string build_id;
  std::string output_dir;
  LedgerEntry ledger;
  VerificationReport verification;
  std::vector<std::string> dep_manifests;    // 相对产物根的 dep-manifest 路径
  std::vector<std::string> skipped_dep_scans;  // 跳过的矩阵 family（镜像锁定失败）
};

class BuildOrchestrator {
 public:
  explicit BuildOrchestrator(const Config& config) : config_(config) {}

  [[nodiscard]] Result<BuildResult> build(const BuildRequest& req);

 private:
  const Config& config_;
};

}  // namespace w2ucem

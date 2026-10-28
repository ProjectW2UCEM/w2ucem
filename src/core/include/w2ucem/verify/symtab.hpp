// 符号表校验：readelf -Ws 原始输出 → 确定性规范化快照。
// 架构决策：规范化器单一实现（宿主 C++），容器内只跑一条原始命令——
// 构建与 verify 共用同一规范化器，杜绝双实现漂移；容器脚本无 awk/sed 列解析，
// busybox 兼容面最小。
//
// 快照格式（确定性论证）：
//   丢弃 Num/Value/Size（链接布局函数，跨机器不稳定且无语义价值）；
//   Ndx 归一化：UND/ABS 保留（导入面敏感），数字 → DEF；
//   行 <Bind> <Type> <Vis> <Ndx> <Name>，全行 sort+unique（集合语义）；
//   头部 3 行：w2ucem-symtab v1 / tool: readelf / scope: <dyn|full>。
//   敏感面 = 导入导出符号增删、类型/绑定/可见性/名称变化。
#pragma once

#include <string>

#include "w2ucem/build/backend.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem::verify {

// scope: "dyn" | "full"（非法值返回 SchemaViolation）。
[[nodiscard]] Result<std::string> canonicalize_symtab(const std::string& readelf_raw,
                                                      const std::string& scope);

// 提取接口：对产物文件产出 readelf -Ws 原始输出。
class SymtabExtractor {
 public:
  virtual ~SymtabExtractor() = default;
  // artifact_rel_path 相对产物根。
  [[nodiscard]] virtual Result<std::string> extract_raw(
      const std::string& artifact_rel_path) = 0;
  [[nodiscard]] virtual std::string tool_name() const = 0;
};

// 生产路径：用账本记录的 buildenv 镜像起容器跑 readelf（verify 复验环境）。
class ContainerSymtabExtractor final : public SymtabExtractor {
 public:
  ContainerSymtabExtractor(ContainerBackend& backend, const std::string& image_ref,
                           const std::string& output_dir);

  [[nodiscard]] Result<std::string> extract_raw(
      const std::string& artifact_rel_path) override;
  [[nodiscard]] std::string tool_name() const override { return "readelf"; }

 private:
  ContainerBackend& backend_;
  std::string image_ref_;
  std::string output_dir_;
};

// 实验路径：宿主 PATH 找 readelf；找不到 extract_raw 返回 Unsupported。
class HostSymtabExtractor final : public SymtabExtractor {
 public:
  HostSymtabExtractor();

  [[nodiscard]] Result<std::string> extract_raw(
      const std::string& artifact_rel_path) override;
  [[nodiscard]] std::string tool_name() const override { return "readelf"; }

 private:
  std::string readelf_path_;  // 空 = 不可用
};

// 快照生成：extractor 为空/不可用 → 空串（调用方决定放行或报"无法复验"）。
class SymtabChecker {
 public:
  explicit SymtabChecker(SymtabExtractor* extractor = nullptr) : extractor_(extractor) {}

  [[nodiscard]] std::string snapshot(const std::string& artifact_rel_path,
                                     const std::string& scope);

 private:
  SymtabExtractor* extractor_;
};

}  // namespace w2ucem::verify

// 装配步骤：有序执行序列的步骤类型。
// alpha 支持 extract / patch / dll-override / copy / dep-scan / manifest；
// 编译型 build 步骤（compile/hybrid）为 alpha 后扩展（format 版本化后门）。
#pragma once

#include <string>
#include <variant>
#include <vector>

namespace w2ucem {

struct ExtractStep {
  std::string source;  // source id
  std::string to;      // 解包目标（支持 $build/$prefix 变量）
};

struct PatchStep {
  std::string patch;  // 补丁文件路径
  std::string dir;    // 应用目录
};

struct DllOverrideStep {
  std::vector<std::string> rules;  // "xaudio2_7=native,builtin" 形式
  std::string output;              // 生成的 override 清单路径（确定性文本生成）
};

struct CopyStep {
  std::string from;
  std::string to;
};

struct DepScanStep {
  std::vector<std::string> elfs;  // 待扫描 ELF 列表
  std::string output;             // 每发行版专属依赖清单路径
};

struct ManifestStep {
  std::vector<std::string> required;  // 汇总断言：列出的路径必须存在
};

using AssembleStep =
    std::variant<ExtractStep, PatchStep, DllOverrideStep, CopyStep, DepScanStep, ManifestStep>;

[[nodiscard]] const char* assemble_step_type(const AssembleStep& step);

}  // namespace w2ucem

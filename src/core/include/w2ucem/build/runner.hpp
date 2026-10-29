// RunnerScript：宿主侧确定性生成容器执行脚本（/bin/sh）。
// 内容仅由 BuildPlan 与矩阵信息决定——不读环境/时间/宿主文件系统；
// 脚本 sha256 记入账本（"容器里执行了什么"是可复现性账本的一部分）。
//
// 容器内路径约定：
//   $build  → /build   （构建工作区，读写挂载）
//   $prefix → /out     （产物根，读写挂载）
//   源文件  → /sources/<sha256>（只读挂载，哈希已校验）
//   相对产物根的路径约定见各生成函数。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "w2ucem/template/build_plan.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct RunnerScript {
  // 生成 buildenv 容器内的装配脚本（extract/patch/dll-override/copy/manifest 步骤）。
  // with_symtab_extraction：是否生成 symtab 原始提取段（fake 后端在无 readelf 的
  // 宿主直跑时必须为 false——跳过决策必须下沉到脚本生成，否则脚本在装配时失败）。
  [[nodiscard]] static std::string generate_assemble(
      const BuildPlan& plan, const std::map<std::string, std::string>& source_id_to_sha256,
      bool with_symtab_extraction = true);

  // 生成矩阵容器内的 dep-scan 脚本（readelf 解析 DT_NEEDED → JSON）。
  // output_path：容器内输出路径（如 /out/dep-manifest.json）。
  [[nodiscard]] static std::string generate_dep_scan(const std::vector<std::string>& elfs,
                                                     const std::string& family,
                                                     const std::string& output_path);
};

}  // namespace w2ucem

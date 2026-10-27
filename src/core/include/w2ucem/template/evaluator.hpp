// TemplateEvaluator：产品核心——"函数式模板"的函数体。
// evaluate(template, target, template_hash) -> BuildPlan，纯函数：
// 不读环境变量、不读时间、不读宿主文件系统，只认入参。
// 规则：
//   1. 域检查：channel/gpu/family 不在模板声明域 → EvaluationError
//   2. 通道策略：stable/major + record manifest → EvaluationError
//   3. 变体合并：[variant.<gpu>] 在 TOML 层深合并（标量/数组替换、
//      表递归、未提及键继承），合并结果重新解析校验
//   4. build_depends 按 target family 查表解析为最终包名列表
#pragma once

#include <string>

#include "w2ucem/channel/target.hpp"
#include "w2ucem/template/build_plan.hpp"
#include "w2ucem/template/template.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct TemplateEvaluator {
  [[nodiscard]] static Result<BuildPlan> evaluate(const Template& tpl,
                                                  const BuildTarget& target,
                                                  const std::string& template_hash);
};

}  // namespace w2ucem

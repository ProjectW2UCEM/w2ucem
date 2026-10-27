// TemplateValidator：模板级交叉校验（求值前执行）。
// 通道策略红线：声明 stable/major 通道的模板禁止 record 模式 manifest。
#pragma once

#include "w2ucem/template/template.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct TemplateValidator {
  [[nodiscard]] static Result<void> validate(const Template& tpl);
};

}  // namespace w2ucem

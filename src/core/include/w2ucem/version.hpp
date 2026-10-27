// W2UCEM 版本标识。
// evaluator_version 与 template_format_version 是"函数式模板"语义版本：
// 求值器（函数体）随库版本锁定，模板（纯数据）声明其 format 版本。
#pragma once

namespace w2ucem {

inline constexpr const char* W2UCEM_VERSION = "0.1.0-alpha";

// 模板求值器语义版本：求值规则变化时必须递增。
inline constexpr const char* EVALUATOR_VERSION = "0.1";

// 模板格式规范版本：与 docs/SPEC.md 对应。
inline constexpr const char* TEMPLATE_FORMAT_VERSION = "0.1";

// C ABI 版本：alpha 不承诺 ABI 稳定，1.0 前可变更。
inline constexpr int W2UCEM_CAPI_VERSION = 0;

}  // namespace w2ucem

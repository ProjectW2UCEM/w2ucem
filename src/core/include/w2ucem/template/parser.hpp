// TemplateParser：TOML → Template。
// 严格 schema：未知键 = 硬错误（拼错键静默忽略在可复现系统里就是构建漂移）。
// 错误携带 key 位置上下文（toml11 错误信息含行号）。
#pragma once

#include <string>

#include "w2ucem/template/template.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct TemplateParser {
  // toml_bytes：模板原始字节（哈希校验针对此字节序列）。
  // context：错误信息上下文（模板 id 或路径）。
  [[nodiscard]] static Result<Template> parse(const std::string& toml_bytes,
                                              const std::string& context);
};

}  // namespace w2ucem

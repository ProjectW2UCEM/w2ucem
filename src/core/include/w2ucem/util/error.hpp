// 错误码与错误结构：整个核心库统一的错误传播载体。
// 错误码覆盖"可复现性校验链"的全部失败面——HashMismatch / SignatureInvalid
// 等是产品承诺的硬失败语义，调用方必须能精确区分。
#pragma once

#include <string>

namespace w2ucem {

enum class ErrorCode {
  None,
  Io,                  // 文件系统错误
  Parse,               // 格式解析失败（TOML/JSON 语法层）
  SchemaViolation,     // schema 违反（未知键、缺字段、类型错）
  EvaluationError,     // 求值失败（域外参数、变体合并冲突）
  HashMismatch,        // 源/补丁/模板/产物哈希不匹配 —— 可复现性硬失败
  SignatureInvalid,    // 仓库索引签名无效
  Network,             // 下载/网络错误
  MirrorExhausted,     // 全部镜像失败且无 Archive 回退
  ContainerNotFound,   // 容器引擎不可用
  ContainerFailure,    // 容器执行失败
  VerificationFailed,  // 产物校验链失败
  Unsupported,         // 未实现/不支持的功能
};

struct Error {
  ErrorCode code{ErrorCode::None};
  std::string message;
  std::string context;  // 上下文：URL / 路径 / 模板 id / build_id

  static Error make(ErrorCode c, std::string msg, std::string ctx = {}) {
    return Error{c, std::move(msg), std::move(ctx)};
  }
};

[[nodiscard]] const char* error_code_str(ErrorCode code);

}  // namespace w2ucem

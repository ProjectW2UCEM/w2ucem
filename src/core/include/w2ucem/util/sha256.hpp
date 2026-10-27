// Sha256：OpenSSL EVP 封装的 SHA-256 计算与 hex 往返。
// 可复现性校验链的基石：源/补丁/模板/产物/runner 脚本全部经此哈希。
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "w2ucem/util/result.hpp"

namespace w2ucem {

using Sha256Digest = std::array<std::uint8_t, 32>;

struct Sha256 {
  // 流式计算文件哈希（64KB 分块，不整体载入内存）。
  [[nodiscard]] static Result<Sha256Digest> of_file(const std::string& path);

  [[nodiscard]] static Sha256Digest of_bytes(const void* data, std::size_t len);
  [[nodiscard]] static Sha256Digest of_bytes(const std::string& data);

  [[nodiscard]] static std::string hex(const Sha256Digest& digest);

  // 解析 64 位 hex 字符串；非法格式返回 SchemaViolation 错误。
  [[nodiscard]] static Result<Sha256Digest> from_hex(const std::string& hex);
  [[nodiscard]] static bool is_valid_hex(const std::string& hex);

  // 字符串往返：hex(d) -> from_hex -> d
  [[nodiscard]] static Result<Sha256Digest> parse_or_error(const std::string& hex,
                                                           const std::string& context);
};

}  // namespace w2ucem

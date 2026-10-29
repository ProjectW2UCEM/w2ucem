// 镜像 digest 锁：pull-by-digest → inspect 实际 digest → 与声明比对。
// mismatch = 硬失败（HashMismatch），保证"锁定 SDK 层"的承诺。
#pragma once

#include <string>

#include "w2ucem/build/backend.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct ImageDigestLock {
  // 返回实际 digest（已验证 == declared）。
  [[nodiscard]] static Result<std::string> pull_and_verify(ContainerBackend& backend,
                                                           const std::string& image,
                                                           const std::string& declared_digest);
};

}  // namespace w2ucem

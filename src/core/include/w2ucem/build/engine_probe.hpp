// 引擎探测与选择：auto 模式按 docker → podman → fake 顺序。
#pragma once

#include <memory>

#include "w2ucem/build/backend.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct EngineProbe {
  // engine: "auto" | "docker" | "podman" | "fake"
  // auto 依次探测 docker/podman；全部不可用时回退 fake 并记录警告。
  [[nodiscard]] static Result<std::shared_ptr<ContainerBackend>> select(const std::string& engine);
};

}  // namespace w2ucem

#include "w2ucem/build/engine_probe.hpp"

#include "w2ucem/build/container_backends.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/logging.hpp"

namespace w2ucem {

Result<std::shared_ptr<ContainerBackend>> EngineProbe::select(const std::string& engine) {
  if (engine == "docker" || engine == "podman") {
    auto backend = std::make_shared<ContainerCliBackend>(engine);
    auto info = backend->probe();
    if (!info) {
      return tl::unexpected(std::move(info.error()));
    }
    Logger::instance().info("容器引擎: {} {}", info->name, info->version);
    return backend;
  }
  if (engine == "fake") {
    return std::make_shared<FakeBackend>();
  }
  if (engine != "auto") {
    return tl::unexpected(
        Error::make(ErrorCode::ContainerNotFound, "未知容器引擎: " + engine));
  }

  // auto：docker → podman → fake
  for (const char* candidate : {"docker", "podman"}) {
    auto backend = std::make_shared<ContainerCliBackend>(candidate);
    auto info = backend->probe();
    if (info) {
      Logger::instance().info("容器引擎（auto）: {} {}", info->name, info->version);
      return backend;
    }
  }
  Logger::instance().warn(
      "未找到 docker/podman，回退 fake 后端（宿主直跑，仅适用于实验/测试路径）");
  return std::make_shared<FakeBackend>();
}

}  // namespace w2ucem

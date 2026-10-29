// 容器 CLI 后端：docker / podman 共用（CLI 兼容），fake 后端：宿主直跑 runner 脚本。
#pragma once

#include "w2ucem/build/backend.hpp"

namespace w2ucem {

// docker 与 podman 的 CLI 兼容：仅命令名不同。
class ContainerCliBackend final : public ContainerBackend {
 public:
  explicit ContainerCliBackend(std::string engine) : engine_(std::move(engine)) {}

  [[nodiscard]] std::string name() const override { return engine_; }
  [[nodiscard]] Result<EngineInfo> probe() override;
  [[nodiscard]] Result<std::string> pull_image(const std::string& image,
                                               const std::string& digest) override;
  [[nodiscard]] Result<ContainerResult> run(const ContainerSpec& spec) override;

 private:
  std::string engine_;
};

// fake 后端：不依赖任何容器引擎，直接以宿主 shell 执行 command。
// 用途：单测、macOS 实验性构建路径（dep-scan 等 Linux 专属步骤会被跳过）。
class FakeBackend final : public ContainerBackend {
 public:
  [[nodiscard]] std::string name() const override { return "fake"; }
  [[nodiscard]] Result<EngineInfo> probe() override;
  [[nodiscard]] Result<std::string> pull_image(const std::string& image,
                                               const std::string& digest) override;
  [[nodiscard]] Result<ContainerResult> run(const ContainerSpec& spec) override;
};

}  // namespace w2ucem

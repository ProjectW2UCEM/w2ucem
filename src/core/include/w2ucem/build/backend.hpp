// 容器引擎后端抽象：docker / podman（CLI 子进程封装，不引 SDK）/ fake（宿主直跑）。
// 设计约束：容器零网络（--network none），只挂载已验证字节（只读）。
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct ContainerMount {
  std::string host_path;
  std::string container_path;
  bool read_only{true};
};

struct ContainerSpec {
  std::string image;  // 完整引用（image@sha256:...）
  std::vector<ContainerMount> mounts;
  std::vector<std::string> env;       // "K=V"
  std::string workdir;
  std::vector<std::string> command;   // 容器内执行命令（argv）
  bool network_disabled{true};
};

struct ContainerResult {
  int exit_code{0};
  std::string log;  // stdout+stderr
};

struct EngineInfo {
  std::string name;
  std::string version;
};

class ContainerBackend {
 public:
  virtual ~ContainerBackend() = default;

  [[nodiscard]] virtual std::string name() const = 0;

  // 探测引擎可用性；不可用返回 ContainerNotFound。
  [[nodiscard]] virtual Result<EngineInfo> probe() = 0;

  // 按 digest 拉取镜像并返回 inspect 到的实际 digest（用于与声明比对）。
  [[nodiscard]] virtual Result<std::string> pull_image(const std::string& image,
                                                       const std::string& digest) = 0;

  [[nodiscard]] virtual Result<ContainerResult> run(const ContainerSpec& spec) = 0;
};

}  // namespace w2ucem

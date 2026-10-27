// process：子进程执行（fork/exec + 输出捕获）。
// 容器引擎后端（docker/podman CLI 封装）与 fake 后端共用。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/util/result.hpp"

namespace w2ucem::process {

struct ProcResult {
  int exit_code{0};
  std::string output;  // stdout+stderr 合并
};

// argv[0] 为可执行文件；context 用于错误信息。
[[nodiscard]] Result<ProcResult> run(const std::vector<std::string>& argv,
                                     const std::string& context);

}  // namespace w2ucem::process

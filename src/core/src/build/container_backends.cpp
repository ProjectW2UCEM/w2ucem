#include "w2ucem/build/container_backends.hpp"

#include <algorithm>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/logging.hpp"
#include "w2ucem/util/process.hpp"

namespace w2ucem {

namespace {

std::string shell_escape(const std::string& s) {
  std::string out = "'";
  for (const char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += '\'';
  return out;
}

}  // namespace

// ---------- ContainerCliBackend ----------

Result<EngineInfo> ContainerCliBackend::probe() {
  auto r = process::run({engine_, "version", "--format", "{{.Server.Version}}"},
                        engine_ + " version");
  if (!r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (r->exit_code != 0) {
    return tl::unexpected(Error::make(ErrorCode::ContainerNotFound,
                                      engine_ + " 不可用: " + r->output, engine_));
  }
  EngineInfo info;
  info.name = engine_;
  info.version = r->output;
  // 去掉首尾空白
  while (!info.version.empty() && (info.version.back() == '\n' || info.version.back() == '\r')) {
    info.version.pop_back();
  }
  return info;
}

Result<std::string> ContainerCliBackend::pull_image(const std::string& image,
                                                    const std::string& digest) {
  const std::string ref = image + "@" + digest;

  // 本地命中检查（e2e/本地构建场景：镜像在 daemon 里，pull 走 registry 反而失败）。
  // 路径 1：按名字 inspect，镜像内容 ID（.Id）== 声明的 digest
  //（e2e 用内容 ID 锁；Linux 原生 docker 对本地镜像的 RepoDigests 为空）。
  auto by_name = process::run(
      {engine_, "image", "inspect", "--format", "{{.Id}}", image}, engine_ + " inspect " + image);
  if (by_name && by_name->exit_code == 0) {
    std::string id = by_name->output;
    while (!id.empty() && (id.back() == '\n' || id.back() == '\r')) {
      id.pop_back();
    }
    if (id == digest) {
      return digest;
    }
  }

  // 路径 2：按 digest 引用 inspect（manifest digest 场景：真实 registry 锁）。
  auto by_ref = process::run(
      {engine_, "image", "inspect", "--format", "{{index .RepoDigests 0}}", ref},
      engine_ + " inspect " + ref);
  if (by_ref && by_ref->exit_code == 0) {
    std::string actual = by_ref->output;
    while (!actual.empty() && (actual.back() == '\n' || actual.back() == '\r')) {
      actual.pop_back();
    }
    const auto at = actual.rfind('@');
    if (at != std::string::npos) {
      actual = actual.substr(at + 1);
    }
    if (!actual.empty()) {
      return actual;
    }
  }

  auto pull = process::run({engine_, "image", "pull", ref}, engine_ + " pull " + ref);
  if (!pull) {
    return tl::unexpected(std::move(pull.error()));
  }
  if (pull->exit_code != 0) {
    return tl::unexpected(Error::make(ErrorCode::ContainerFailure,
                                      "镜像拉取失败: " + pull->output, ref));
  }

  auto inspect = process::run(
      {engine_, "image", "inspect", "--format", "{{index .RepoDigests 0}}", ref},
      engine_ + " inspect " + ref);
  if (!inspect || inspect->exit_code != 0) {
    return tl::unexpected(Error::make(ErrorCode::ContainerFailure,
                                      "镜像 inspect 失败: " +
                                          (inspect ? inspect->output : inspect.error().message),
                                      ref));
  }
  std::string actual = inspect->output;
  while (!actual.empty() && (actual.back() == '\n' || actual.back() == '\r')) {
    actual.pop_back();
  }
  // RepoDigests 形如 "image@sha256:..."，取 @ 后 digest 部分
  const auto at = actual.rfind('@');
  if (at != std::string::npos) {
    actual = actual.substr(at + 1);
  }
  return actual;
}

Result<ContainerResult> ContainerCliBackend::run(const ContainerSpec& spec) {
  std::vector<std::string> argv{engine_, "run", "--rm"};
  if (spec.network_disabled) {
    argv.push_back("--network");
    argv.push_back("none");
  }
  for (const auto& m : spec.mounts) {
    argv.push_back("-v");
    argv.push_back(m.host_path + ":" + m.container_path + (m.read_only ? ":ro" : ""));
  }
  for (const auto& e : spec.env) {
    argv.push_back("-e");
    argv.push_back(e);
  }
  if (!spec.workdir.empty()) {
    argv.push_back("-w");
    argv.push_back(spec.workdir);
  }
  argv.push_back(spec.image);
  for (const auto& c : spec.command) {
    argv.push_back(c);
  }

  auto r = process::run(argv, engine_ + " run");
  if (!r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (r->exit_code != 0) {
    Logger::instance().error("容器执行失败（{}）: {}", engine_, r->output);
    return tl::unexpected(Error::make(ErrorCode::ContainerFailure,
                                      "容器执行失败，退出码 " + std::to_string(r->exit_code),
                                      spec.image));
  }
  return ContainerResult{r->exit_code, r->output};
}

// ---------- FakeBackend ----------

Result<EngineInfo> FakeBackend::probe() {
  return EngineInfo{"fake", "0"};
}

Result<std::string> FakeBackend::pull_image(const std::string& image,
                                            const std::string& digest) {
  // 不真拉取；返回声明 digest（记录进账本）。
  Logger::instance().warn("fake 后端跳过镜像拉取: {} @ {}", image, digest);
  return digest;
}

Result<ContainerResult> FakeBackend::run(const ContainerSpec& spec) {
  // 在宿主直接执行 command（sh -c）；容器路径映射到挂载的宿主路径。
  if (spec.command.empty()) {
    return tl::unexpected(Error::make(ErrorCode::ContainerFailure, "fake 后端空命令", "fake"));
  }
  std::string cmdline;
  for (const auto& c : spec.command) {
    cmdline += shell_escape(c) + " ";
  }
  // 容器路径 → 宿主路径替换（按最长路径优先，避免前缀误替换）
  std::vector<ContainerMount> mounts = spec.mounts;
  std::sort(mounts.begin(), mounts.end(), [](const ContainerMount& a, const ContainerMount& b) {
    return a.container_path.size() > b.container_path.size();
  });
  std::string expanded = cmdline;
  for (const auto& m : mounts) {
    std::size_t pos = 0;
    while ((pos = expanded.find(m.container_path, pos)) != std::string::npos) {
      // 边界检查：前后须非路径字符（/、空白、引号、行尾）
      const bool left_ok =
          pos == 0 || expanded[pos - 1] == '/' || expanded[pos - 1] == ' ' ||
          expanded[pos - 1] == '\'' || expanded[pos - 1] == '\n';
      const auto end = pos + m.container_path.size();
      const bool right_ok = end >= expanded.size() || expanded[end] == '/' ||
                            expanded[end] == ' ' || expanded[end] == '\'' ||
                            expanded[end] == '\n';
      if (left_ok && right_ok) {
        expanded.replace(pos, m.container_path.size(), m.host_path);
        pos += m.host_path.size();
      } else {
        pos += m.container_path.size();
      }
    }
  }
  const auto r = process::run({"/bin/sh", "-c", expanded}, "fake run");
  if (!r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (r->exit_code != 0) {
    Logger::instance().error("fake 后端执行失败: {}", r->output);
    return tl::unexpected(Error::make(ErrorCode::ContainerFailure,
                                      "fake 后端执行失败，退出码 " + std::to_string(r->exit_code),
                                      expanded));
  }
  return ContainerResult{r->exit_code, r->output};
}

}  // namespace w2ucem

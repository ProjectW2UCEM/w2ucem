#include "w2ucem/util/process.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>

#include "w2ucem/util/error.hpp"

namespace w2ucem::process {

Result<ProcResult> run(const std::vector<std::string>& argv, const std::string& context) {
  if (argv.empty()) {
    return tl::unexpected(Error::make(ErrorCode::Io, "空命令", context));
  }

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    return tl::unexpected(Error::make(ErrorCode::Io, "pipe 失败: " + std::string(std::strerror(errno)),
                                      context));
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return tl::unexpected(Error::make(ErrorCode::Io,
                                      "fork 失败: " + std::string(std::strerror(errno)), context));
  }

  if (pid == 0) {
    // 子进程：stdout+stderr → pipe，exec
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& a : argv) {
      args.push_back(const_cast<char*>(a.c_str()));
    }
    args.push_back(nullptr);
    execvp(args[0], args.data());
    _exit(127);  // exec 失败
  }

  // 父进程
  close(pipefd[1]);
  std::string output;
  std::array<char, 4096> buf{};
  ssize_t n = 0;
  while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
    output.append(buf.data(), static_cast<std::size_t>(n));
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  ProcResult result;
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  result.output = std::move(output);
  return result;
}

}  // namespace w2ucem::process

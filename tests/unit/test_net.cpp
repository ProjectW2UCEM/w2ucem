// 断点续传单测：fork/exec 本地 python mock 服务器（loopback，无外网依赖）。
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/net.hpp"
#include "w2ucem/util/sha256.hpp"

using namespace w2ucem;

namespace {

// mock 服务器夹具：fork 子进程跑 http_mock_server.py，读首行 PORT。
// 构造失败记录到 ok_/err_（Catch2 REQUIRE 不能用于构造函数），测试用 REQUIRE(server.ok())。
class MockHttpServer {
 public:
  MockHttpServer(const std::string& mode, const std::string& file_path,
                 std::size_t flaky_bytes = 0, const std::string& record_path = "")
      : record_path_(record_path) {
    std::filesystem::remove(record_path_);

    int pipefd[2];
    if (pipe(pipefd) != 0) {
      err_ = "pipe 失败";
      return;
    }
    pid_ = fork();
    if (pid_ < 0) {
      err_ = "fork 失败";
      close(pipefd[0]);
      close(pipefd[1]);
      return;
    }
    if (pid_ == 0) {
      // 子进程：stdout → pipe，exec python 服务器
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[0]);
      close(pipefd[1]);
      std::vector<std::string> args = {"python3", script_path(), "--file", file_path,
                                       "--mode", mode};
      if (flaky_bytes > 0) {
        args.push_back("--flaky-bytes");
        args.push_back(std::to_string(flaky_bytes));
      }
      if (!record_path.empty()) {
        args.push_back("--record");
        args.push_back(record_path);
      }
      std::vector<char*> argv;
      for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
      }
      argv.push_back(nullptr);
      execvp(argv[0], argv.data());
      _exit(127);
    }

    // 父进程：读 PORT 行
    close(pipefd[1]);
    std::string line;
    char buf[1];
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, 1)) > 0) {
      if (buf[0] == '\n') {
        break;
      }
      line += buf[0];
    }
    close(pipefd[0]);
    if (line.rfind("PORT ", 0) != 0) {
      err_ = "服务器未输出端口行";
      kill(pid_, SIGKILL);
      waitpid(pid_, nullptr, 0);
      pid_ = -1;
      return;
    }
    port_ = std::stoi(line.substr(5));
    ok_ = true;
  }

  ~MockHttpServer() {
    if (pid_ > 0) {
      kill(pid_, SIGKILL);
      waitpid(pid_, nullptr, 0);
    }
  }

  [[nodiscard]] bool ok() const { return ok_; }
  [[nodiscard]] const std::string& err() const { return err_; }

  [[nodiscard]] std::string url() const {
    return "http://127.0.0.1:" + std::to_string(port_) + "/file";
  }

  [[nodiscard]] bool record_seen() const {
    return record_path_.empty() ? false : fs::exists(record_path_);
  }

 private:
  static std::string script_path() {
    return std::string(TEST_SOURCE_DIR) + "/http_mock_server.py";
  }

  pid_t pid_{-1};
  int port_{0};
  bool ok_{false};
  std::string err_;
  std::string record_path_;
};

// 确定性测试数据：256KB 循环字节。
std::string make_payload(std::size_t size = 256 * 1024) {
  std::string data;
  data.reserve(size);
  for (std::size_t i = 0; i < size; ++i) {
    data += static_cast<char>(i % 251);
  }
  return data;
}

struct TempDir {
  std::filesystem::path path;

  TempDir() {
    path = std::filesystem::temp_directory_path() /
           ("w2ucem-net-test-" + std::to_string(getpid()));
    std::filesystem::create_directories(path);
  }
  ~TempDir() { std::filesystem::remove_all(path); }

  std::string file(const std::string& name) const { return (path / name).string(); }
};

void write_file(const std::string& path, const std::string& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

TEST_CASE("续传：全新下载") {
  TempDir dir;
  const auto payload = make_payload();
  write_file(dir.file("data.bin"), payload);
  MockHttpServer server("normal", dir.file("data.bin"));
  REQUIRE(server.ok());

  const auto dst = dir.file("out.bin");
  const auto r = net::get_to_file(server.url(), dst);
  if (!r) {
    INFO("下载错误: " << r.error().message << " [" << r.error().context << "]");
  }
  REQUIRE(r);
  REQUIRE(fs::read_file(dst));
  REQUIRE(*fs::read_file(dst) == payload);
}

TEST_CASE("续传：预置 partial → 206 续传完成") {
  TempDir dir;
  const auto payload = make_payload();
  write_file(dir.file("data.bin"), payload);
  const auto record = dir.file("record.txt");
  MockHttpServer server("normal", dir.file("data.bin"), 0, record);
  REQUIRE(server.ok());

  const auto dst = dir.file("out.bin");
  write_file(dst, payload.substr(0, 32 * 1024));  // 预置 32KB partial

  REQUIRE(net::get_to_file(server.url(), dst));
  REQUIRE(*fs::read_file(dst) == payload);
  REQUIRE(server.record_seen());  // 确实走了 206 续传路径
}

TEST_CASE("续传：服务器忽略 Range（200 全量）→ 回退无拼接污染") {
  TempDir dir;
  const auto payload = make_payload();
  write_file(dir.file("data.bin"), payload);
  MockHttpServer server("no-range", dir.file("data.bin"));
  REQUIRE(server.ok());

  const auto dst = dir.file("out.bin");
  write_file(dst, payload.substr(0, 16 * 1024));

  const auto r = net::get_to_file(server.url(), dst);
  if (!r) {
    INFO("下载错误: " << r.error().message << " [" << r.error().context << "]");
  }
  REQUIRE(r);
  const auto result = fs::read_file(dst);
  REQUIRE(result);
  REQUIRE(*result == payload);  // 必须等于 payload 而非 partial+payload 拼接
}

TEST_CASE("续传：416（partial 越界）→ 回退全量") {
  TempDir dir;
  const auto payload = make_payload();
  write_file(dir.file("data.bin"), payload);
  MockHttpServer server("normal", dir.file("data.bin"));
  REQUIRE(server.ok());

  const auto dst = dir.file("out.bin");
  // 预置完整大小的 partial：服务器对 Range 越界回 416 → 回退全量重下
  write_file(dst, payload);

  REQUIRE(net::get_to_file(server.url(), dst));
  REQUIRE(*fs::read_file(dst) == payload);
}

TEST_CASE("续传：206 截断后跨调用续传") {
  TempDir dir;
  const auto payload = make_payload();
  write_file(dir.file("data.bin"), payload);

  const auto dst = dir.file("out.bin");
  write_file(dst, payload.substr(0, 32 * 1024));

  {
    // flaky 服务器：206 只写 64KB 后断连接 → partial 增长 64KB 且内容正确
    MockHttpServer flaky("flaky", dir.file("data.bin"), 64 * 1024);
    REQUIRE(flaky.ok());
    const auto r = net::get_to_file(flaky.url(), dst);
    REQUIRE(!r);
    const auto size = fs::file_size(dst);
    REQUIRE(size);
    REQUIRE(*size == 32 * 1024 + 64 * 1024);
  }

  {
    // 换 normal 服务器：从 96KB 处续传
    MockHttpServer normal("normal", dir.file("data.bin"));
    REQUIRE(normal.ok());
    REQUIRE(net::get_to_file(normal.url(), dst));
    REQUIRE(*fs::read_file(dst) == payload);
  }
}

TEST_CASE("续传：416 回退全量也失败 → 错误且 dst 被删除") {
  TempDir dir;
  const auto payload = make_payload(1024);
  write_file(dir.file("data.bin"), payload);
  MockHttpServer server("always-416", dir.file("data.bin"));
  REQUIRE(server.ok());

  const auto dst = dir.file("out.bin");
  write_file(dst, payload.substr(0, 512));

  // partial 512 > 0 → 续传得 416 → 回退全量 → 全量仍 416 → 错误且 dst 删除
  const auto r = net::get_to_file(server.url(), dst);
  REQUIRE(!r);
  REQUIRE(!fs::exists(dst));  // 错误页不保留
}

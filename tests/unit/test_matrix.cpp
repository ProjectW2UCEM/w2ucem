// 矩阵表解析单测（load_from_file 绕过进程级单例）。
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include <unistd.h>

#include "w2ucem/build/matrix.hpp"

using namespace w2ucem;

namespace {

std::string write_temp(const std::string& content, const std::string& name) {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("w2ucem-matrix-test-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir);
  const auto path = dir / name;
  {
    std::ofstream out(path);
    out << content;
  }
  return path.string();
}

}  // namespace

TEST_CASE("矩阵：四族解析与未知族忽略") {
  const std::string content = R"(
[deb]
image = "w2ucem/matrix-debian"
digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000"

[rpm]
image = "w2ucem/matrix-fedora"
digest = "sha256:1111111111111111111111111111111111111111111111111111111111111111"

[arch]
image = "w2ucem/matrix-archlinux"
digest = "sha256:2222222222222222222222222222222222222222222222222222222222222222"

[alpine]
image = "w2ucem/matrix-alpine"
digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333"

[suse]
image = "whatever"
digest = "sha256:4444444444444444444444444444444444444444444444444444444444444444"
)";
  const auto path = write_temp(content, "matrix.toml");

  const auto cells = DistroMatrix::load_from_file(path);
  REQUIRE(cells);
  REQUIRE(cells->size() == 4);  // 未知族被忽略
  REQUIRE(cells->at(DistroFamily::Debian).image == "w2ucem/matrix-debian");
  REQUIRE(cells->at(DistroFamily::Rpm).digest.rfind("sha256:1111", 0) == 0);
  REQUIRE(cells->at(DistroFamily::Arch).image == "w2ucem/matrix-archlinux");
  REQUIRE(cells->at(DistroFamily::Alpine).digest.rfind("sha256:3333", 0) == 0);
  REQUIRE(cells->count(static_cast<DistroFamily>(99)) == 0);
}

TEST_CASE("矩阵：非法 TOML → Parse 错误") {
  const auto path = write_temp("not [valid toml\n", "bad.toml");
  const auto cells = DistroMatrix::load_from_file(path);
  REQUIRE(!cells);
  REQUIRE(cells.error().code == ErrorCode::Parse);
}

TEST_CASE("矩阵：缺字段 → Parse 错误") {
  const auto path = write_temp("[deb]\nimage = \"x\"\n", "missing.toml");
  const auto cells = DistroMatrix::load_from_file(path);
  REQUIRE(!cells);
  REQUIRE(cells.error().code == ErrorCode::Parse);
}

TEST_CASE("矩阵：仓库真实 ci/matrix.toml 防漂移") {
  // 编译默认路径：构建目录 generated/matrix.toml（configure_file 拷贝）
  const std::string path = std::string(W2UCEM_MATRIX_FILE);
  const auto cells = DistroMatrix::load_from_file(path);
  REQUIRE(cells);
  REQUIRE(cells->size() == 4);
  for (const auto family : {DistroFamily::Debian, DistroFamily::Rpm, DistroFamily::Arch,
                            DistroFamily::Alpine}) {
    REQUIRE(cells->count(family) == 1);
    REQUIRE(!cells->at(family).image.empty());
    REQUIRE(cells->at(family).digest.rfind("sha256:", 0) == 0);
  }
}

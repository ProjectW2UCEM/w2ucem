#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/sha256.hpp"

using namespace w2ucem;

TEST_CASE("sha256 已知向量") {
  // NIST 测试向量
  REQUIRE(Sha256::hex(Sha256::of_bytes("")) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  REQUIRE(Sha256::hex(Sha256::of_bytes("abc")) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  REQUIRE(Sha256::hex(Sha256::of_bytes("hello world")) ==
          "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

TEST_CASE("sha256 hex 往返") {
  const auto digest = Sha256::of_bytes("abc");
  const auto hex = Sha256::hex(digest);
  REQUIRE(hex.size() == 64);
  REQUIRE(Sha256::is_valid_hex(hex));

  const auto back = Sha256::from_hex(hex);
  REQUIRE(back);
  REQUIRE(*back == digest);
}

TEST_CASE("sha256 非法 hex 拒绝") {
  REQUIRE(!Sha256::from_hex("zzzz"));
  REQUIRE(!Sha256::from_hex("ba7816bf"));  // 长度不足
  REQUIRE(!Sha256::from_hex(std::string(64, 'g')));
  REQUIRE(!Sha256::is_valid_hex("XYZ"));
}

TEST_CASE("sha256 流式文件哈希") {
  const auto dir = std::filesystem::temp_directory_path() / "w2ucem-sha-test";
  std::filesystem::create_directories(dir);
  const auto path = (dir / "data.bin").string();
  {
    std::ofstream out(path, std::ios::binary);
    const std::string payload(200000, 'x');  // 超过 64KB 分块
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  }
  const auto file_digest = Sha256::of_file(path);
  REQUIRE(file_digest);
  REQUIRE(*file_digest == Sha256::of_bytes(std::string(200000, 'x')));

  const auto missing = Sha256::of_file(path + ".nope");
  REQUIRE(!missing);
  REQUIRE(missing.error().code == ErrorCode::Io);

  std::filesystem::remove_all(dir);
}

TEST_CASE("错误码字符串覆盖全枚举") {
  for (int i = 0; i <= static_cast<int>(ErrorCode::Unsupported); ++i) {
    REQUIRE(std::string(error_code_str(static_cast<ErrorCode>(i))) != "unknown");
  }
}

TEST_CASE("原子写与读回") {
  const auto dir = std::filesystem::temp_directory_path() / "w2ucem-fs-test";
  std::filesystem::create_directories(dir);
  const auto path = (dir / "f.txt").string();

  REQUIRE(fs::atomic_write(path, "第一版"));
  REQUIRE(fs::read_file(path));
  REQUIRE(*fs::read_file(path) == "第一版");

  REQUIRE(fs::atomic_write(path, "第二版更长内容"));
  REQUIRE(*fs::read_file(path) == "第二版更长内容");

  REQUIRE(fs::exists(path));
  REQUIRE(fs::is_dir(dir.string()));
  REQUIRE(!fs::is_dir(path));

  std::filesystem::remove_all(dir);
}

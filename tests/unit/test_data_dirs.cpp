#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "w2ucem/config/data_dirs.hpp"

using namespace w2ucem;

TEST_CASE("DataDirs root 重定向与布局") {
  const auto tmp = (std::filesystem::temp_directory_path() / "w2ucem-dirs-test").string();
  std::filesystem::remove_all(tmp);

  const auto dirs = DataDirs::create(tmp);
  REQUIRE(dirs);
  REQUIRE(dirs->root() == tmp);
  REQUIRE(dirs->index_path() == tmp + "/sync/repo.db");
  REQUIRE(dirs->index_sig_path() == tmp + "/sync/repo.db.sig");
  REQUIRE(dirs->sources_cache() == tmp + "/cache/sources");
  REQUIRE(dirs->archive_blob_dir() == tmp + "/archive/blob");
  REQUIRE(dirs->plugins_dir() == tmp + "/lib/plugins");
  REQUIRE(dirs->user_templates_dir() == tmp + "/lib/user-templates");
  REQUIRE(dirs->default_pubkey_path() == tmp + "/trust/pubkey.pem");

  // create 后 root 目录必须已存在
  REQUIRE(std::filesystem::exists(tmp));

  std::filesystem::remove_all(tmp);
}

TEST_CASE("runtime_dir 按 target 维度分层") {
  const auto tmp = (std::filesystem::temp_directory_path() / "w2ucem-dirs-test2").string();
  std::filesystem::remove_all(tmp);
  const auto dirs = DataDirs::create(tmp);
  REQUIRE(dirs);

  const BuildTarget target{Channel::Stable, GpuVariant::Nvidia,
                           Distro{DistroFamily::Arch, "rolling"}, "x86_64"};
  REQUIRE(dirs->runtime_dir("proton-ge", target) ==
          tmp + "/runtime/proton-ge/stable/arch/nvidia");
  REQUIRE(dirs->ledger_dir("proton-ge", target) ==
          tmp + "/runtime/proton-ge/stable/arch/nvidia/ledger");

  std::filesystem::remove_all(tmp);
}

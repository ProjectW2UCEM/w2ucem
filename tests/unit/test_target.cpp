#include <catch2/catch_test_macros.hpp>

#include "w2ucem/channel/target.hpp"

using namespace w2ucem;

TEST_CASE("TargetParser 规范四段") {
  const auto t = TargetParser::parse("stable:nvidia:deb:bookworm");
  REQUIRE(t);
  REQUIRE(t->channel == Channel::Stable);
  REQUIRE(t->gpu == GpuVariant::Nvidia);
  REQUIRE(t->distro.family == DistroFamily::Debian);
  REQUIRE(t->distro.release == "bookworm");
  REQUIRE(t->arch == "x86_64");
}

TEST_CASE("TargetParser 三段缺省 release") {
  const auto arch = TargetParser::parse("semi-rolling:generic:arch");
  REQUIRE(arch);
  REQUIRE(arch->distro.family == DistroFamily::Arch);
  REQUIRE(arch->distro.release == "rolling");

  const auto rpm = TargetParser::parse("major:amd:rpm");
  REQUIRE(rpm);
  REQUIRE(rpm->distro.release == "fedora");

  const auto alpine = TargetParser::parse("stable:generic:alpine");
  REQUIRE(alpine);
  REQUIRE(alpine->distro.release == "edge");
}

TEST_CASE("TargetParser 拒绝非法输入") {
  REQUIRE(!TargetParser::parse("nightly:generic:deb"));
  REQUIRE(!TargetParser::parse("stable:qualcomm:deb"));
  REQUIRE(!TargetParser::parse("stable:generic:suse"));
  REQUIRE(!TargetParser::parse("a:b:c:d:e"));
  REQUIRE(!TargetParser::parse(""));
}

TEST_CASE("TargetParser to_string 往返") {
  const auto t = TargetParser::parse("major:amd:rpm:39");
  REQUIRE(t);
  REQUIRE(TargetParser::to_string(*t) == "major:amd:rpm:39");

  const auto back = TargetParser::parse(TargetParser::to_string(*t));
  REQUIRE(back);
  REQUIRE(back->channel == t->channel);
  REQUIRE(back->gpu == t->gpu);
  REQUIRE(back->distro.family == t->distro.family);
  REQUIRE(back->distro.release == t->distro.release);
}

TEST_CASE("channel/variant/family 字符串往返") {
  REQUIRE(channel_from_str("semi-rolling") == Channel::SemiRolling);
  REQUIRE(channel_from_str("stable") == Channel::Stable);
  REQUIRE(channel_from_str("major") == Channel::Major);
  REQUIRE(!channel_from_str("rolling"));

  REQUIRE(gpu_variant_from_str("generic") == GpuVariant::Generic);
  REQUIRE(gpu_variant_from_str("intel") == GpuVariant::Intel);
  REQUIRE(!gpu_variant_from_str("qualcomm"));

  REQUIRE(family_from_str("deb") == DistroFamily::Debian);
  REQUIRE(family_from_str("rpm") == DistroFamily::Rpm);
  REQUIRE(family_from_str("arch") == DistroFamily::Arch);
  REQUIRE(family_from_str("alpine") == DistroFamily::Alpine);
  REQUIRE(!family_from_str("suse"));

  REQUIRE(std::string(family_str(DistroFamily::Arch)) == "arch");
  REQUIRE(std::string(channel_str(Channel::Major)) == "major");
  REQUIRE(std::string(gpu_variant_str(GpuVariant::Amd)) == "amd");
}

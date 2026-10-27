#include <catch2/catch_test_macros.hpp>

#include "w2ucem/template/evaluator.hpp"
#include "w2ucem/template/parser.hpp"
#include "w2ucem/template/validator.hpp"
#include "w2ucem/version.hpp"

using namespace w2ucem;

namespace {

// 其他测试片段（普通字符串拼接）共用：合法 64 位 hex 占位值。
const std::string DIGEST = "sha256:" + std::string(64, '0');

// 合法装配型模板底稿：sha256 占位值均为合法 64 位 hex。
// 注意：raw string 内不得出现 )" 序列（会提前终止），因此哈希直接写字面量。
std::string valid_template_toml() {
  return R"(
[template]
format = "0.1"
id = "proton-ge"
name = "Proton-GE assembly"
version = "9.27"
kind = "assembly"
upstream = "https://github.com/GloriousEggroll/proton-ge-custom"

[meta]
maintainer = "w2ucem team <dev@w2ucem.org>"
license = "BSD-3-Clause"
channels = ["semi-rolling", "stable", "major"]
gpu_variants = ["generic", "nvidia"]
distro_families = ["arch", "deb", "rpm", "alpine"]

[buildenv]
image = "w2ucem/buildenv-alpine"
digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000"

[sources.ge-proton-9-27]
urls = ["https://github.com/GloriousEggroll/proton-ge-custom/releases/download/GE-Proton9-27/GE-Proton9-27.tar.gz", "https://mirror.w2ucem.org/proton-ge/GE-Proton9-27.tar.gz"]
sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
archive_id = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

[build_depends]
default = []
arch = ["libx11", "fontconfig"]
deb = ["libx11-dev", "libfontconfig1-dev"]
rpm = ["libX11-devel", "fontconfig-devel"]

[[assemble.steps]]
type = "extract"
source = "ge-proton-9-27"
to = "$build/src"

[[assemble.steps]]
type = "dll-override"
rules = ["xaudio2_7=native,builtin", "d3dcompiler_47=native,builtin"]
output = "dll-overrides.json"

[[assemble.steps]]
type = "copy"
from = "$build/src/files"
to = "$prefix"

[[assemble.steps]]
type = "dep-scan"
elfs = ["$prefix/files/bin/wine64"]
output = "dep-manifest.json"

[[assemble.steps]]
type = "manifest"
required = ["$prefix/files/bin/wine64", "dll-overrides.json", "dep-manifest.json"]

[variant.nvidia]
[variant.nvidia.build_depends]
arch = ["nvidia-utils", "lib32-nvidia-utils"]

[hooks]
pre-assemble = { plugins = ["w2ucem-debug-log"] }

[manifest]
[[manifest.files]]
path = "files/bin/wine64"
sha256 = ""
[[manifest.files]]
path = "dll-overrides.json"
sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
)";
}

Result<Template> parse_valid(const std::string& extra = "") {
  return TemplateParser::parse(valid_template_toml() + extra, "test");
}

// 通道收窄为仅 semi-rolling：record 模式 manifest 在 stable/major 下被禁，
// 因此"合法模板"的标准做法是只声明 semi-rolling。
std::string semi_rolling_only() {
  auto toml = valid_template_toml();
  const std::string from = "channels = [\"semi-rolling\", \"stable\", \"major\"]";
  const auto pos = toml.find(from);
  if (pos == std::string::npos) {
    return "";
  }
  toml.replace(pos, from.size(), "channels = [\"semi-rolling\"]");
  return toml;
}

// 进一步收窄取值域：仅 deb 族、仅 generic 变体（域外求值测试用）。
std::string deb_only_semi_rolling() {
  auto toml = semi_rolling_only();
  const std::string from = "distro_families = [\"arch\", \"deb\", \"rpm\", \"alpine\"]";
  const auto pos = toml.find(from);
  if (pos == std::string::npos) {
    return "";
  }
  toml.replace(pos, from.size(), "distro_families = [\"deb\"]");
  return toml;
}

}  // namespace

// ---------- 解析与 schema ----------

TEST_CASE("合法装配模板解析+校验通过") {
  const auto tpl = TemplateParser::parse(semi_rolling_only(), "test");
  REQUIRE(tpl);
  REQUIRE(tpl->id == "proton-ge");
  REQUIRE(tpl->kind == "assembly");
  REQUIRE(tpl->sources.size() == 1);
  REQUIRE(tpl->sources.at("ge-proton-9-27").urls.size() == 2);
  REQUIRE(tpl->build_depends.per_family.at(DistroFamily::Arch).size() == 2);
  REQUIRE(tpl->steps.size() == 5);
  REQUIRE(tpl->steps[0].index() == 0);  // ExtractStep
  REQUIRE(tpl->variants.count("nvidia") == 1);
  REQUIRE(tpl->hooks.plugins.count(HookPoint::PreAssemble) == 1);
  REQUIRE(tpl->manifest.files.size() == 2);

  REQUIRE(TemplateValidator::validate(*tpl));
}

TEST_CASE("未知键 = 硬错误（schema 红线）") {
  // 顶层未知键
  auto r = TemplateParser::parse("[templat]\nformat=\"0.1\"\n", "t");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);

  // [template] 表内未知键
  r = TemplateParser::parse(valid_template_toml() + "\n[templatte]\nx = 1\n", "t");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);

  // 步骤内未知键
  r = TemplateParser::parse(
      valid_template_toml() + "\n[[assemble.steps]]\ntype = \"extract\"\nsource = \"a\"\nto = \"b\"\nextra_field = 1\n",
      "t");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);
}

TEST_CASE("缺表与类型错误拒绝") {
  auto r = TemplateParser::parse("x = 1", "t");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);

  r = TemplateParser::parse(
      "[template]\nformat=\"0.1\"\nid=\"x\"\nversion=\"1\"\nkind=\"assembly\"\n[buildenv]\nimage=\"i\"\ndigest=\""
          + DIGEST + "\"\n",
      "t");
  REQUIRE(!r);  // 缺 [meta]
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);

  // 类型错误：channels 应为数组
  r = TemplateParser::parse(
      "[template]\nformat=\"0.1\"\nid=\"x\"\nversion=\"1\"\nkind=\"assembly\"\n[meta]\nchannels=\"stable\"\ngpu_variants=[\"generic\"]\ndistro_families=[\"deb\"]\n[buildenv]\nimage=\"i\"\ndigest=\""
          + DIGEST + "\"\n",
      "t");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);
}

TEST_CASE("sha256 / digest 非法拒绝") {
  auto r = TemplateParser::parse(
      "[template]\nformat=\"0.1\"\nid=\"x\"\nversion=\"1\"\nkind=\"assembly\"\n[meta]\nchannels=[\"stable\"]\ngpu_variants=[\"generic\"]\ndistro_families=[\"deb\"]\n[buildenv]\nimage=\"i\"\ndigest=\"sha256:not-hex\"\n",
      "t");
  REQUIRE(r);
  REQUIRE(!TemplateValidator::validate(*r));  // digest 非法

  r = TemplateParser::parse(
      valid_template_toml() + R"(
[sources.bad]
urls = ["https://example.com/x.tar.gz"]
sha256 = "oops"
)",
      "t");
  REQUIRE(r);
  REQUIRE(!TemplateValidator::validate(*r));  // 源 sha256 非法
}

TEST_CASE("kind=compile 与 format 版本拒绝") {
  auto r = TemplateParser::parse(
      "[template]\nformat=\"0.1\"\nid=\"x\"\nversion=\"1\"\nkind=\"compile\"\n[meta]\nchannels=[\"stable\"]\ngpu_variants=[\"generic\"]\ndistro_families=[\"deb\"]\n[buildenv]\nimage=\"i\"\ndigest=\""
          + DIGEST + "\"\n",
      "t");
  REQUIRE(r);
  auto v = TemplateValidator::validate(*r);
  REQUIRE(!v);
  REQUIRE(v.error().code == ErrorCode::Unsupported);

  r = TemplateParser::parse(
      "[template]\nformat=\"9.9\"\nid=\"x\"\nversion=\"1\"\nkind=\"assembly\"\n[meta]\nchannels=[\"stable\"]\ngpu_variants=[\"generic\"]\ndistro_families=[\"deb\"]\n[buildenv]\nimage=\"i\"\ndigest=\""
          + DIGEST + "\"\n",
      "t");
  REQUIRE(r);
  v = TemplateValidator::validate(*r);
  REQUIRE(!v);
  REQUIRE(v.error().code == ErrorCode::Unsupported);
}

TEST_CASE("variant 键必须声明在取值域") {
  auto r = TemplateParser::parse(valid_template_toml() + R"(
[variant.amd]
build_depends = { arch = ["x"] }
)",
                                 "t");
  REQUIRE(r);
  const auto v = TemplateValidator::validate(*r);
  REQUIRE(!v);
  REQUIRE(v.error().code == ErrorCode::SchemaViolation);
}

TEST_CASE("stable/major 通道禁 record 模式 manifest") {
  // 基础模板声明了 stable 且 manifest 有 record 条目 → validator 拒绝
  const auto tpl = parse_valid();
  REQUIRE(tpl);
  const auto v = TemplateValidator::validate(*tpl);
  REQUIRE(!v);
  REQUIRE(v.error().code == ErrorCode::SchemaViolation);
  REQUIRE(v.error().message.find("record") != std::string::npos);
}

// ---------- 求值 ----------

TEST_CASE("域外 channel/gpu/family 求值拒绝") {
  // 取值域收窄为 {semi-rolling, generic, deb}
  const auto tpl = TemplateParser::parse(deb_only_semi_rolling(), "t");
  REQUIRE(tpl);

  BuildTarget ok{Channel::SemiRolling, GpuVariant::Generic,
                 Distro{DistroFamily::Debian, "stable"}, "x86_64"};
  REQUIRE(TemplateEvaluator::evaluate(*tpl, ok, "hash123"));

  BuildTarget bad_channel{Channel::Stable, GpuVariant::Generic,
                          Distro{DistroFamily::Debian, "stable"}, "x86_64"};
  auto r = TemplateEvaluator::evaluate(*tpl, bad_channel, "hash123");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::EvaluationError);

  BuildTarget bad_gpu{Channel::SemiRolling, GpuVariant::Intel,
                      Distro{DistroFamily::Debian, "stable"}, "x86_64"};
  r = TemplateEvaluator::evaluate(*tpl, bad_gpu, "hash123");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::EvaluationError);

  BuildTarget bad_family{Channel::SemiRolling, GpuVariant::Generic,
                         Distro{DistroFamily::Arch, "rolling"}, "x86_64"};
  r = TemplateEvaluator::evaluate(*tpl, bad_family, "hash123");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::EvaluationError);
}

TEST_CASE("build_depends 按 family 解析（含 default 回退）") {
  const auto tpl = TemplateParser::parse(semi_rolling_only(), "t");
  REQUIRE(tpl);

  BuildTarget deb{Channel::SemiRolling, GpuVariant::Generic,
                  Distro{DistroFamily::Debian, "stable"}, "x86_64"};
  auto plan = TemplateEvaluator::evaluate(*tpl, deb, "hash123");
  REQUIRE(plan);
  REQUIRE(plan->build_dep_packages ==
          std::vector<std::string>{"libx11-dev", "libfontconfig1-dev"});

  // alpine 无显式条目 → default（空）
  BuildTarget alpine{Channel::SemiRolling, GpuVariant::Generic,
                     Distro{DistroFamily::Alpine, "edge"}, "x86_64"};
  plan = TemplateEvaluator::evaluate(*tpl, alpine, "hash123");
  REQUIRE(plan);
  REQUIRE(plan->build_dep_packages.empty());
}

TEST_CASE("变体深合并：nvidia 覆盖 build_depends（表递归 + 数组替换）") {
  const auto tpl = TemplateParser::parse(semi_rolling_only(), "t");
  REQUIRE(tpl);

  BuildTarget nvidia{Channel::SemiRolling, GpuVariant::Nvidia,
                     Distro{DistroFamily::Arch, "rolling"}, "x86_64"};
  const auto plan = TemplateEvaluator::evaluate(*tpl, nvidia, "hash123");
  REQUIRE(plan);
  // 数组整体替换：nvidia 变体覆盖 arch → 只剩 nvidia 两项
  REQUIRE(plan->build_dep_packages ==
          std::vector<std::string>{"nvidia-utils", "lib32-nvidia-utils"});
  // 未提及键继承：deb 条目来自 base
  BuildTarget nvidia_deb{Channel::SemiRolling, GpuVariant::Nvidia,
                         Distro{DistroFamily::Debian, "stable"}, "x86_64"};
  const auto plan2 = TemplateEvaluator::evaluate(*tpl, nvidia_deb, "hash123");
  REQUIRE(plan2);
  REQUIRE(plan2->build_dep_packages ==
          std::vector<std::string>{"libx11-dev", "libfontconfig1-dev"});
}

TEST_CASE("BuildPlan 字段与 to_json") {
  const auto tpl = TemplateParser::parse(semi_rolling_only(), "t");
  REQUIRE(tpl);

  BuildTarget target{Channel::SemiRolling, GpuVariant::Generic,
                     Distro{DistroFamily::Arch, "rolling"}, "x86_64"};
  const auto plan = TemplateEvaluator::evaluate(*tpl, target, "deadbeef");
  REQUIRE(plan);
  REQUIRE(plan->template_id == "proton-ge");
  REQUIRE(plan->template_hash == "deadbeef");
  REQUIRE(plan->evaluator_version == std::string(EVALUATOR_VERSION));
  REQUIRE(plan->sources.size() == 1);
  REQUIRE(plan->sources[0].urls.size() == 2);
  REQUIRE(plan->steps.size() == 5);
  REQUIRE(plan->manifest.files.size() == 2);
  REQUIRE(plan->manifest.has_record_mode());  // wine64 条目为 record

  const auto json = plan->to_json();
  REQUIRE(json.find("\"template_id\": \"proton-ge\"") != std::string::npos);
  REQUIRE(json.find("\"target\"") != std::string::npos);
  REQUIRE(json.find("\"evaluator_version\"") != std::string::npos);
  REQUIRE(json.find("\"build_dep_packages\"") != std::string::npos);
  REQUIRE(json.find("\"libx11\"") != std::string::npos);
}

TEST_CASE("stable 求值 + record manifest → EvaluationError") {
  // 基础模板声明了 stable 且含 record：validator 已拒绝；
  // 这里验证通过构造绕过 validator 直接求值的双保险。
  // 实际路径：evaluate 内部对合并结果 validate，等价覆盖。
  const auto tpl = parse_valid();
  REQUIRE(tpl);
  BuildTarget stable{Channel::Stable, GpuVariant::Generic,
                     Distro{DistroFamily::Debian, "stable"}, "x86_64"};
  const auto r = TemplateEvaluator::evaluate(*tpl, stable, "h");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::EvaluationError);
}

TEST_CASE("TOML 语法错误 → Parse") {
  const auto r = TemplateParser::parse("[template\nformat = \"0.1\"", "t");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::Parse);
}

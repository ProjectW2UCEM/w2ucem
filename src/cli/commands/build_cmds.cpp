// build / verify / ledger 命令（构建流水线 CLI 面）。
#include <CLI/CLI.hpp>

#include "output.hpp"
#include "w2ucem/api/facade.hpp"
#include "w2ucem/channel/target.hpp"
#include "w2ucem/util/error.hpp"

namespace w2ucem::cli {

void add_build_commands(CLI::App& app, const std::string& config_path,
                        const std::string& root_override) {
  const auto init_facade = [&]() {
    return w2ucem::api::Facade::init(config_path, root_override);
  };

  // ---------- build ----------
  auto* build = app.add_subcommand("build", "求值 + 容器构建（核心命令）");
  static std::string build_ref;
  static std::string build_target = TargetParser::to_string(BuildTarget{});
  static std::string engine = "auto";
  static bool full_matrix = false;
  static bool no_verify = false;
  build->add_option("template", build_ref, "模板引用 id[@version]")->required();
  build->add_option("--target", build_target, "求值目标 channel:gpu:family:release");
  build->add_option("--engine", engine, "容器引擎 auto|docker|podman|fake");
  build->add_flag("--distro-matrix", full_matrix, "全部矩阵发行版跑 dep-scan");
  build->add_flag("--no-verify", no_verify, "跳过产物校验链");
  build->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto target = TargetParser::parse(build_target);
    if (!target) {
      fmt::print("{}\n", format_error(target.error()));
      return;
    }
    auto result = facade->build_template(build_ref, *target, engine, full_matrix, !no_verify);
    if (!result) {
      fmt::print("{}\n", format_error(result.error()));
      return;
    }
    fmt::print("构建完成\n");
    fmt::print("  build_id:  {}\n", result->build_id);
    fmt::print("  产物目录:  {}\n", result->output_dir);
    fmt::print("  校验链:    {} 项检查通过\n", result->verification.checks.size());
    for (const auto& dep : result->dep_manifests) {
      fmt::print("  依赖清单:  {}\n", dep);
    }
    for (const auto& skipped : result->skipped_dep_scans) {
      fmt::print("  跳过(镜像锁定失败):  {}\n", skipped);
    }
  });

  // ---------- verify ----------
  auto* verify = app.add_subcommand("verify", "对已有产物跑校验链");
  static std::string verify_ref;
  static std::string verify_target = TargetParser::to_string(BuildTarget{});
  verify->add_option("template", verify_ref, "模板引用 id[@version]")->required();
  verify->add_option("--target", verify_target, "求值目标 channel:gpu:family:release");
  verify->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto target = TargetParser::parse(verify_target);
    if (!target) {
      fmt::print("{}\n", format_error(target.error()));
      return;
    }
    auto report = facade->verify_template(verify_ref, *target);
    if (!report) {
      fmt::print("{}\n", format_error(report.error()));
      return;
    }
    for (const auto& c : report->checks) {
      fmt::print("{} {}\n", c.ok ? "[ok]" : "[!!]", c.name + (c.detail.empty() ? "" : " — " + c.detail));
    }
    if (!report->all_ok()) {
      fmt::print("校验失败\n");
    }
  });

  // ---------- ledger ----------
  auto* ledger = app.add_subcommand("ledger", "查看可复现性账本");
  static std::string ledger_ref;
  static std::string ledger_target = TargetParser::to_string(BuildTarget{});
  ledger->add_option("template", ledger_ref, "模板引用 id[@version]");
  ledger->add_option("--target", ledger_target, "求值目标 channel:gpu:family:release");
  ledger->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    if (ledger_ref.empty()) {
      auto all = facade->list_ledgers();
      if (!all) {
        fmt::print("{}\n", format_error(all.error()));
        return;
      }
      for (const auto& [tmpl, target, build_id] : *all) {
        fmt::print("{}  {}  {}\n", build_id, tmpl, target);
      }
      return;
    }
    auto target = TargetParser::parse(ledger_target);
    if (!target) {
      fmt::print("{}\n", format_error(target.error()));
      return;
    }
    auto entry = facade->latest_ledger(ledger_ref, *target);
    if (!entry) {
      fmt::print("{}\n", format_error(entry.error()));
      return;
    }
    if (!*entry) {
      fmt::print("(该 target 无账本记录)\n");
      return;
    }
    fmt::print("{}\n", (*entry)->to_json());
  });
}

}  // namespace w2ucem::cli

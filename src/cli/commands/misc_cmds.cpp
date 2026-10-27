// doctor / version 命令。
#include <CLI/CLI.hpp>

#include "output.hpp"
#include "w2ucem/api/facade.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/version.hpp"

namespace w2ucem::cli {

void add_misc_commands(CLI::App& app, const std::string& config_path,
                       const std::string& root_override) {
  // ---------- doctor ----------
  auto* doctor = app.add_subcommand("doctor", "环境自检：引擎、镜像、数据目录、仓库连通");
  doctor->callback([&]() {
    auto facade = w2ucem::api::Facade::init(config_path, root_override);
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    const auto report = facade->doctor();
    for (const auto& c : report.checks) {
      fmt::print("{} {:<10} {}\n", c.ok ? "[ok]" : "[!!]", c.name, c.detail);
    }
    fmt::print("\n");
  });

  // ---------- version ----------
  auto* version = app.add_subcommand("version", "显示版本信息");
  static bool json_out = false;
  version->add_flag("--json", json_out, "机器可读输出");
  version->callback([&]() {
    if (json_out) {
      fmt::print("{{\"w2ucem\":\"{}\",\"evaluator\":\"{}\",\"format\":\"{}\",\"capi\":{}}}\n",
                 w2ucem::W2UCEM_VERSION, w2ucem::EVALUATOR_VERSION,
                 w2ucem::TEMPLATE_FORMAT_VERSION, w2ucem::W2UCEM_CAPI_VERSION);
    } else {
      fmt::print("w2ucem {}\n", w2ucem::W2UCEM_VERSION);
      fmt::print("  evaluator: {}\n", w2ucem::EVALUATOR_VERSION);
      fmt::print("  format:    {}\n", w2ucem::TEMPLATE_FORMAT_VERSION);
      fmt::print("  capi:      {}\n", w2ucem::W2UCEM_CAPI_VERSION);
    }
  });
}

}  // namespace w2ucem::cli

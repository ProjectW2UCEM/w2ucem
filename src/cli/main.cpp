// w2ucem-cli 入口：命令分发（pacman 风格）。
#include <CLI/CLI.hpp>

#include "commands/output.hpp"
#include "w2ucem/util/logging.hpp"

namespace w2ucem::cli {
void add_repo_commands(CLI::App& app, const std::string& config_path,
                       const std::string& root_override);
void add_build_commands(CLI::App& app, const std::string& config_path,
                        const std::string& root_override);
void add_user_commands(CLI::App& app, const std::string& config_path,
                       const std::string& root_override);
void add_misc_commands(CLI::App& app, const std::string& config_path,
                       const std::string& root_override);
}  // namespace w2ucem::cli

int main(int argc, char** argv) {
  CLI::App app{"W2UCEM — Windows to Unix Compatibility Environment Manager"};
  app.require_subcommand(1);

  std::string config_path;
  std::string root_override;
  bool verbose = false;
  app.add_option("--config", config_path, "配置文件路径（TOML）");
  app.add_option("--root", root_override, "数据目录重定向（测试/隔离，pacman -r 风格）");
  app.add_flag("--verbose,-v", verbose, "详细日志");

  w2ucem::cli::add_repo_commands(app, config_path, root_override);
  w2ucem::cli::add_build_commands(app, config_path, root_override);
  w2ucem::cli::add_user_commands(app, config_path, root_override);
  w2ucem::cli::add_misc_commands(app, config_path, root_override);

  CLI11_PARSE(app, argc, argv);
  return 0;
}

// user-template / plugin 命令（AUR 式用户模板体系 CLI 面）。
#include <CLI/CLI.hpp>

#include "output.hpp"
#include "w2ucem/api/facade.hpp"
#include "w2ucem/util/error.hpp"

namespace w2ucem::cli {

void add_user_commands(CLI::App& app, const std::string& config_path,
                       const std::string& root_override) {
  const auto init_facade = [&]() {
    return w2ucem::api::Facade::init(config_path, root_override);
  };

  // ---------- user-template ----------
  auto* ut = app.add_subcommand("user-template", "用户模板（AUR 式）体系");

  // check：模板 schema + 哈希校验 + 元数据/心跳状态检查
  auto* ut_check = ut->add_subcommand("check", "校验用户模板（schema+哈希+状态）");
  static std::string check_path;
  ut_check->add_option("path", check_path, "用户模板 TOML 路径")->required();
  ut_check->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto result = facade->ut_check(check_path);
    if (!result) {
      fmt::print("{}\n", format_error(result.error()));
      return;
    }
    fmt::print("用户模板校验通过: {}\n", check_path);
  });

  // heartbeat：更新心跳（状态机驱动）
  auto* ut_hb = ut->add_subcommand("heartbeat", "更新维护者心跳（每周签到）");
  static std::string hb_id;
  ut_hb->add_option("id", hb_id, "用户模板 id")->required();
  ut_hb->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto meta = facade->ut_heartbeat(hb_id);
    if (!meta) {
      fmt::print("{}\n", format_error(meta.error()));
      return;
    }
    fmt::print("心跳已更新: {} → {} ({} / {})\n", meta->id,
               w2ucem::user_status_str(meta->status), meta->last_heartbeat,
               w2ucem::review_status_str(meta->review_status));
  });

  // list：注册表（默认隐藏封存条目）
  auto* ut_list = ut->add_subcommand("list", "列出用户模板注册表");
  static bool ut_show_archived = false;
  ut_list->add_flag("--include-archived", ut_show_archived, "包含封存条目");
  ut_list->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto metas = facade->ut_list(ut_show_archived);
    if (!metas) {
      fmt::print("{}\n", format_error(metas.error()));
      return;
    }
    if (metas->empty()) {
      fmt::print("(空)\n");
      return;
    }
    for (const auto& meta : *metas) {
      fmt::print("{:<20} {} 心跳: {} 审核: {}\n", meta.id,
                 w2ucem::user_status_str(meta.status), meta.last_heartbeat,
                 w2ucem::review_status_str(meta.review_status));
    }
  });

  // ---------- plugin ----------
  auto* plugin = app.add_subcommand("plugin", "插件系统");
  auto* plugin_list = plugin->add_subcommand("list", "列出已安装插件");
  plugin_list->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto plugins = facade->plugin_list();
    if (!plugins) {
      fmt::print("{}\n", format_error(plugins.error()));
      return;
    }
    if (plugins->empty()) {
      fmt::print("(无已安装插件)\n");
      return;
    }
    for (const auto& [id, hook] : *plugins) {
      fmt::print("{:<24} hook: {}\n", id, hook);
    }
  });
}

}  // namespace w2ucem::cli

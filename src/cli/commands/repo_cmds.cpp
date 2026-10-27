// 仓库与模板命令：sync / search / info / list / fetch / archive-check。
#include <CLI/CLI.hpp>

#include "output.hpp"
#include "w2ucem/api/facade.hpp"
#include "w2ucem/channel/target.hpp"
#include "w2ucem/util/error.hpp"

namespace w2ucem::cli {

namespace {

std::string default_target_arg() {
  return TargetParser::to_string(BuildTarget{});
}

}  // namespace

void add_repo_commands(CLI::App& app, const std::string& config_path,
                       const std::string& root_override) {
  // 每个命令自行 init Facade（CLI 生命周期简单化）
  const auto init_facade = [&]() {
    return w2ucem::api::Facade::init(config_path, root_override);
  };

  // ---------- sync ----------
  auto* sync = app.add_subcommand("sync", "拉取仓库索引并验签");
  static bool sync_force = false;
  static std::vector<std::string> sync_repos;
  sync->add_flag("--force", sync_force, "强制重新拉取");
  sync->add_option("--repo", sync_repos, "仓库 URL 覆盖（可重复）");
  sync->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    // 先应用 URL（仅内存）→ sync → 成功才持久化（失败不留坏仓库污染后续命令）
    if (!sync_repos.empty()) {
      facade->set_repo_urls(sync_repos);
    }
    auto idx = facade->sync(sync_force);
    if (!idx) {
      fmt::print("{}\n", format_error(idx.error()));
      return;
    }
    if (!sync_repos.empty()) {
      if (auto r = facade->persist_config(); !r) {
        fmt::print("[warn] 仓库 URL 持久化失败: {}\n", format_error(r.error()));
      }
    }
    fmt::print("已同步仓库索引：{} 个模板（schema v{}）\n", idx->templates.size(),
               idx->schema_version);
  });

  // ---------- search ----------
  auto* search = app.add_subcommand("search", "搜索模板");
  static std::string query;
  static bool include_exp = false;
  search->add_option("query", query, "搜索词（id 子串）")->required(false)->default_val("");
  search->add_flag("--include-experimental", include_exp, "包含实验性模板");
  search->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto results = facade->search(query, include_exp);
    if (!results) {
      fmt::print("{}\n", format_error(results.error()));
      return;
    }
    for (const auto& t : *results) {
      fmt::print("{:<24} {}  [{}]\n", t.id + "@" + t.version, t.channels.empty() ? "" : "ch: " + t.channels.front(),
                 t.review_status);
    }
  });

  // ---------- info ----------
  auto* info = app.add_subcommand("info", "模板详情 + 求值预览");
  static std::string info_ref;
  static std::string info_target = default_target_arg();
  info->add_option("template", info_ref, "模板引用 id[@version]")->required();
  info->add_option("--target", info_target, "求值目标 channel:gpu:family:release");
  info->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto target = TargetParser::parse(info_target);
    if (!target) {
      fmt::print("{}\n", format_error(target.error()));
      return;
    }
    auto tpl = facade->template_info(info_ref);
    if (!tpl) {
      fmt::print("{}\n", format_error(tpl.error()));
      return;
    }
    fmt::print("{} {}\n", tpl->id, tpl->version);
    fmt::print("  名称:     {}\n", tpl->name);
    fmt::print("  类型:     {}\n", tpl->kind);
    fmt::print("  上游:     {}\n", tpl->upstream);
    fmt::print("  维护者:   {}\n", tpl->meta.maintainer);
    fmt::print("  通道:     {}\n", fmt::join(tpl->meta.channels, ", "));
    fmt::print("  GPU 变体: {}\n", fmt::join(tpl->meta.gpu_variants, ", "));
    fmt::print("  发行版族: {}\n", fmt::join(tpl->meta.distro_families, ", "));
    fmt::print("  构建镜像: {} @ {}\n", tpl->buildenv.image, tpl->buildenv.digest);
    fmt::print("  源:       {}\n", tpl->sources.size());
    fmt::print("  步骤:     {}\n", tpl->steps.size());

    const auto plan = facade->preview(info_ref, *target);
    if (!plan) {
      fmt::print("  求值预览: 失败 — {}\n", format_error(plan.error()));
      return;
    }
    fmt::print("\n  求值预览（target: {}）:\n", TargetParser::to_string(*target));
    fmt::print("    构建依赖: {}\n",
               plan->build_dep_packages.empty()
                   ? std::string("(无)")
                   : fmt::format("{}", fmt::join(plan->build_dep_packages, ", ")));
    fmt::print("    模板哈希: {}\n", plan->template_hash);
    fmt::print("    求值器:   {}\n", plan->evaluator_version);
  });

  // ---------- list ----------
  auto* list = app.add_subcommand("list", "列出可用模板");
  static bool list_exp = false;
  list->add_flag("--include-experimental", list_exp, "包含实验性模板");
  list->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto ids = facade->list_available(list_exp);
    if (!ids) {
      fmt::print("{}\n", format_error(ids.error()));
      return;
    }
    if (ids->empty()) {
      fmt::print("(空)\n");
      return;
    }
    for (const auto& id : *ids) {
      fmt::print("{}\n", id);
    }
  });

  // ---------- fetch ----------
  auto* fetch = app.add_subcommand("fetch", "只下载并校验源，不构建（pacman -Sw 语义）");
  static std::string fetch_ref;
  static std::string fetch_target = default_target_arg();
  fetch->add_option("template", fetch_ref, "模板引用 id[@version]")->required();
  fetch->add_option("--target", fetch_target, "求值目标 channel:gpu:family:release");
  fetch->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto target = TargetParser::parse(fetch_target);
    if (!target) {
      fmt::print("{}\n", format_error(target.error()));
      return;
    }
    auto reports = facade->fetch_sources(fetch_ref, *target, false);
    if (!reports) {
      fmt::print("{}\n", format_error(reports.error()));
      return;
    }
    for (const auto& r : *reports) {
      fmt::print("已校验  {}  → {}\n", r.source_id, r.local_path);
    }
  });

  // ---------- archive-check ----------
  auto* archive_check =
      app.add_subcommand("archive-check", "诊断：跳过官方源，强制走时光机仓库下载");
  static std::string ac_ref;
  static std::string ac_target = default_target_arg();
  archive_check->add_option("template", ac_ref, "模板引用 id[@version]")->required();
  archive_check->add_option("--target", ac_target, "求值目标 channel:gpu:family:release");
  archive_check->callback([&, init_facade]() {
    auto facade = init_facade();
    if (!facade) {
      fmt::print("{}\n", format_error(facade.error()));
      return;
    }
    auto target = TargetParser::parse(ac_target);
    if (!target) {
      fmt::print("{}\n", format_error(target.error()));
      return;
    }
    const auto reports = facade->fetch_sources(ac_ref, *target, true);
    if (!reports) {
      fmt::print("{}\n", format_error(reports.error()));
      return;
    }
    fmt::print("时光机回退验证通过：全部 {} 个源经 Archive 获取\n", reports->size());
    for (const auto& r : *reports) {
      fmt::print("  {} → {}\n", r.source_id, r.local_path);
    }
  });
}

}  // namespace w2ucem::cli

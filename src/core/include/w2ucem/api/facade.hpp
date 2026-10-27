// Facade：全部模块的单一入口。CLI/TUI/CAPI 唯一依赖面（前端无关机制之二）。
// alpha 的 Facade 面覆盖 P3 命令：sync/search/info/list/fetch/archive-check/doctor；
// P4 扩展 build/verify/ledger；P5 扩展 plugin/user-template。
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "w2ucem/build/ledger.hpp"
#include "w2ucem/build/orchestrator.hpp"
#include "w2ucem/channel/target.hpp"
#include "w2ucem/config/config.hpp"
#include "w2ucem/repo/repo_index.hpp"
#include "w2ucem/template/build_plan.hpp"
#include "w2ucem/template/template.hpp"
#include "w2ucem/usertemplate/meta.hpp"
#include "w2ucem/util/result.hpp"
#include "w2ucem/verify/verifier.hpp"

namespace w2ucem::api {

struct TemplateRef {
  std::string id;
  std::string version;  // 空 = 索引中的最新条目
};

struct FetchReport {
  std::string source_id;
  std::string local_path;  // 已校验源文件路径
};

struct DoctorReport {
  struct Check {
    std::string name;
    bool ok{false};
    std::string detail;
  };
  std::vector<Check> checks;
  [[nodiscard]] bool all_ok() const;
};

class Facade {
 public:
  // config_path 为空则仅用默认配置；root 重定向数据目录（测试/隔离）。
  [[nodiscard]] static Result<Facade> init(const std::string& config_path,
                                           const std::string& root);

  [[nodiscard]] const Config& config() const { return config_; }
  // 覆盖仓库 URL（仅内存）。
  void set_repo_urls(std::vector<std::string> urls);
  // 把当前仓库 URL 持久化到 <root>/config.toml（sync 成功后调用）。
  [[nodiscard]] Result<void> persist_config() const;

  // ---------- 仓库 ----------
  [[nodiscard]] Result<RepoIndex> sync(bool force = false);
  [[nodiscard]] Result<RepoIndex> local_index();
  [[nodiscard]] Result<std::vector<std::string>> list_available(bool include_experimental = false);
  [[nodiscard]] Result<std::vector<IndexedTemplate>> search(const std::string& query,
                                                            bool include_experimental = false);

  // ---------- 模板 ----------
  [[nodiscard]] Result<TemplateRef> parse_ref(const std::string& ref) const;
  [[nodiscard]] Result<Template> template_info(const std::string& ref);       // 下载+校验+解析
  [[nodiscard]] Result<BuildPlan> preview(const std::string& ref, const BuildTarget& target);
  [[nodiscard]] Result<std::string> template_hash(const std::string& ref);    // 原始字节 sha256

  // ---------- 源获取（fetch / archive-check 共用） ----------
  [[nodiscard]] Result<std::vector<FetchReport>> fetch_sources(
      const std::string& ref, const BuildTarget& target, bool force_archive);

  // ---------- 构建 / 校验 / 账本（P4） ----------
  [[nodiscard]] Result<BuildResult> build_template(const std::string& ref,
                                                   const BuildTarget& target,
                                                   const std::string& engine,
                                                   bool full_matrix, bool verify_on);
  [[nodiscard]] Result<VerificationReport> verify_template(const std::string& ref,
                                                           const BuildTarget& target);
  // 全部账本条目（template, target, build_id）
  [[nodiscard]] Result<std::vector<std::tuple<std::string, std::string, std::string>>>
  list_ledgers();
  [[nodiscard]] Result<std::optional<LedgerEntry>> latest_ledger(const std::string& ref,
                                                                 const BuildTarget& target);

  // ---------- 用户模板 / 插件（P5） ----------
  // 校验用户模板文件（schema + 哈希合法性），不写入注册表。
  [[nodiscard]] Result<void> ut_check(const std::string& path);
  [[nodiscard]] Result<UserTemplateMeta> ut_heartbeat(const std::string& id);
  [[nodiscard]] Result<std::vector<UserTemplateMeta>> ut_list(bool include_archived);
  [[nodiscard]] Result<std::vector<std::pair<std::string, std::string>>> plugin_list();

  // ---------- 环境自检 ----------
  [[nodiscard]] DoctorReport doctor();

 private:
  explicit Facade(Config config) : config_(std::move(config)) {}

  // 解析 ref → 索引条目 + 模板（字节已校验）
  [[nodiscard]] Result<std::pair<IndexedTemplate, std::string>> resolve_template(
      const std::string& ref);

  Config config_;
};

}  // namespace w2ucem::api

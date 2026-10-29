#include "w2ucem/api/facade.hpp"

#include <cstdlib>
#include <ctime>
#include <sstream>

#include <memory>

#include "w2ucem/archive/fallback_resolver.hpp"
#include "w2ucem/build/engine_probe.hpp"
#include "w2ucem/plugin/plugin.hpp"
#include "w2ucem/repo/repo_client.hpp"
#include "w2ucem/template/evaluator.hpp"
#include "w2ucem/template/parser.hpp"
#include "w2ucem/template/validator.hpp"
#include "w2ucem/usertemplate/registry.hpp"
#include "w2ucem/usertemplate/state_machine.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/logging.hpp"
#include "w2ucem/util/net.hpp"
#include "w2ucem/util/sha256.hpp"
#include "w2ucem/verify/symtab.hpp"

namespace w2ucem::api {

namespace {

// PATH 中查找可执行文件（doctor 用）。
bool find_in_path(const std::string& name) {
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return false;
  }
  std::stringstream ss(path_env);
  std::string dir;
  while (std::getline(ss, dir, ':')) {
    if (fs::exists(dir + "/" + name)) {
      return true;
    }
  }
  return false;
}

}  // namespace

Result<Facade> Facade::init(const std::string& config_path, const std::string& root) {
  auto config = ConfigLoader::load(config_path, root);
  if (!config) {
    return tl::unexpected(std::move(config.error()));
  }
  Logger::instance().set_sink(std::make_shared<StderrLogSink>());
  return Facade(std::move(*config));
}

void Facade::set_repo_urls(std::vector<std::string> urls) {
  config_.repo_urls = std::move(urls);
}

Result<void> Facade::persist_config() const {
  std::ostringstream oss;
  oss << "[w2ucem]\nrepo_urls = [";
  for (std::size_t i = 0; i < config_.repo_urls.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << "\"" << config_.repo_urls[i] << "\"";
  }
  oss << "]\n";
  const std::string path = config_.dirs.root() + "/config.toml";
  return fs::atomic_write(path, oss.str());
}

// ---------- 仓库 ----------

Result<RepoIndex> Facade::sync(bool force) {
  (void)force;  // alpha：sync 即全量替换本地索引
  const std::string pubkey = config_.sig_pubkey_file.empty()
                                 ? config_.dirs.default_pubkey_path()
                                 : config_.sig_pubkey_file;
  return RepoClient::sync(config_.repo_urls, pubkey, config_.allow_unsigned_index,
                          config_.dirs.sync_dir());
}

Result<RepoIndex> Facade::local_index() {
  return RepoClient::read_local_index(config_.dirs.sync_dir());
}

Result<std::vector<std::string>> Facade::list_available(bool include_experimental) {
  auto idx = local_index();
  if (!idx) {
    return tl::unexpected(std::move(idx.error()));
  }
  std::vector<std::string> out;
  for (const auto& t : idx->templates) {
    if (!include_experimental && t.review_status == "experimental") {
      continue;
    }
    out.push_back(t.id + "@" + t.version);
  }
  return out;
}

Result<std::vector<IndexedTemplate>> Facade::search(const std::string& query,
                                                    bool include_experimental) {
  auto idx = local_index();
  if (!idx) {
    return tl::unexpected(std::move(idx.error()));
  }
  std::vector<IndexedTemplate> out;
  for (const auto& t : idx->templates) {
    if (!include_experimental && t.review_status == "experimental") {
      continue;
    }
    if (query.empty() || t.id.find(query) != std::string::npos) {
      out.push_back(t);
    }
  }
  return out;
}

// ---------- 模板 ----------

Result<TemplateRef> Facade::parse_ref(const std::string& ref) const {
  const auto at = ref.find('@');
  if (at == std::string::npos) {
    if (ref.empty()) {
      return tl::unexpected(Error::make(ErrorCode::EvaluationError, "模板引用不能为空"));
    }
    return TemplateRef{ref, ""};
  }
  return TemplateRef{ref.substr(0, at), ref.substr(at + 1)};
}

Result<std::pair<IndexedTemplate, std::string>> Facade::resolve_template(
    const std::string& ref) {
  auto refp = parse_ref(ref);
  if (!refp) {
    return tl::unexpected(std::move(refp.error()));
  }
  auto idx = local_index();
  if (!idx) {
    return tl::unexpected(std::move(idx.error()));
  }
  const IndexedTemplate* entry = refp->version.empty() ? idx->find(refp->id)
                                                       : idx->find(refp->id, refp->version);
  if (entry == nullptr) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "模板 '" + ref + "' 不在索引中（先 `w2ucem sync`）"));
  }
  auto bytes = RepoClient::fetch_template(*entry, config_.repo_urls);
  if (!bytes) {
    return tl::unexpected(std::move(bytes.error()));
  }
  return std::pair{*entry, *bytes};
}

Result<Template> Facade::template_info(const std::string& ref) {
  auto rt = resolve_template(ref);
  if (!rt) {
    return tl::unexpected(std::move(rt.error()));
  }
  const std::string ctx = rt->first.id + "@" + rt->first.version;
  auto tpl = TemplateParser::parse(rt->second, ctx);
  if (!tpl) {
    return tl::unexpected(std::move(tpl.error()));
  }
  if (auto v = TemplateValidator::validate(*tpl); !v) {
    return tl::unexpected(std::move(v.error()));
  }
  // 落本地模板缓存（后续 offline 可用；alpha 缓存路径仅信息性）
  const std::string path =
      config_.dirs.templates_dir() + "/" + tpl->id + "-" + tpl->version + ".toml";
  if (auto r = fs::ensure_dir(config_.dirs.templates_dir()); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (auto r = fs::atomic_write(path, rt->second); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  return *tpl;
}

Result<BuildPlan> Facade::preview(const std::string& ref, const BuildTarget& target) {
  auto rt = resolve_template(ref);
  if (!rt) {
    return tl::unexpected(std::move(rt.error()));
  }
  const std::string ctx = rt->first.id + "@" + rt->first.version;
  auto tpl = TemplateParser::parse(rt->second, ctx);
  if (!tpl) {
    return tl::unexpected(std::move(tpl.error()));
  }
  if (auto v = TemplateValidator::validate(*tpl); !v) {
    return tl::unexpected(std::move(v.error()));
  }
  const std::string hash = Sha256::hex(Sha256::of_bytes(rt->second));
  return TemplateEvaluator::evaluate(*tpl, target, hash);
}

// ---------- 源获取 ----------

Result<std::vector<FetchReport>> Facade::fetch_sources(const std::string& ref,
                                                       const BuildTarget& target,
                                                       bool force_archive) {
  auto plan = preview(ref, target);
  if (!plan) {
    return tl::unexpected(std::move(plan.error()));
  }
  std::vector<FetchReport> reports;
  for (const auto& src : plan->sources) {
    auto path = FallbackSourceResolver::resolve(src, config_.archive_urls,
                                                config_.dirs.sources_cache(),
                                                config_.dirs.archive_blob_dir(), force_archive);
    if (!path) {
      return tl::unexpected(std::move(path.error()));
    }
    reports.push_back(FetchReport{src.id, *path});
  }
  return reports;
}

// ---------- 构建 / 校验 / 账本 ----------

Result<BuildResult> Facade::build_template(const std::string& ref, const BuildTarget& target,
                                           const std::string& engine, bool full_matrix,
                                           bool verify_on) {
  auto rt = resolve_template(ref);
  if (!rt) {
    return tl::unexpected(std::move(rt.error()));
  }
  const std::string ctx = rt->first.id + "@" + rt->first.version;
  auto tpl = TemplateParser::parse(rt->second, ctx);
  if (!tpl) {
    return tl::unexpected(std::move(tpl.error()));
  }
  if (auto v = TemplateValidator::validate(*tpl); !v) {
    return tl::unexpected(std::move(v.error()));
  }

  BuildRequest request;
  request.tpl = std::move(*tpl);
  request.template_hash = Sha256::hex(Sha256::of_bytes(rt->second));
  request.target = target;
  request.engine = engine;
  request.full_matrix = full_matrix;
  request.verify_on = verify_on;

  BuildOrchestrator orchestrator(config_);
  return orchestrator.build(request);
}

Result<VerificationReport> Facade::verify_template(const std::string& ref,
                                                   const BuildTarget& target) {
  auto plan = preview(ref, target);
  if (!plan) {
    return tl::unexpected(std::move(plan.error()));
  }
  const std::string output_dir = config_.dirs.runtime_dir(plan->template_id, target);

  std::optional<LedgerEntry> prior;
  auto existing = LedgerStore::list(config_.dirs.ledger_dir(plan->template_id, target));
  if (existing && !existing->empty()) {
    auto last = LedgerStore::load(config_.dirs.ledger_dir(plan->template_id, target),
                                  existing->back());
    if (last && *last) {
      prior = **last;
    }
  }

  auto report = Verifier::verify_artifacts(output_dir, plan->manifest, prior);
  if (prior) {
    auto dep_report = Verifier::verify_dep_manifests(output_dir, prior->dep_manifests);
    report.checks.insert(report.checks.end(), dep_report.checks.begin(), dep_report.checks.end());
    // symtab 复验：声明了 symtab 的条目需要报告（无记录 → "跳过"，有记录 → 容器提取比对）
    bool has_symtab_declared = false;
    for (const auto& f : plan->manifest.files) {
      if (!f.symtab.empty()) {
        has_symtab_declared = true;
        break;
      }
    }
    if (has_symtab_declared) {
      // backend 的所有权必须在 extractor 使用期间存活（extractor 持有其引用）
      std::shared_ptr<ContainerBackend> backend;
      std::unique_ptr<verify::SymtabExtractor> extractor;
      if (!prior->symtabs.empty()) {
        auto backend_r = EngineProbe::select("auto");
        if (backend_r) {
          backend = *backend_r;
          std::string image_ref = prior->buildenv_ref;
          if (image_ref.empty() && !prior->images.empty()) {
            // 旧账本无 buildenv_ref：用 images 首条拼 image@digest
            image_ref = prior->images.front().first + "@" + prior->images.front().second;
          }
          if (!image_ref.empty()) {
            extractor = std::make_unique<verify::ContainerSymtabExtractor>(
                *backend, image_ref, output_dir);
          }
        }
      }
      auto symtab_report =
          Verifier::verify_symtabs(output_dir, plan->manifest, prior, extractor.get());
      report.checks.insert(report.checks.end(), symtab_report.checks.begin(),
                           symtab_report.checks.end());
    }
  }
  if (!report.all_ok()) {
    return tl::unexpected(Error::make(ErrorCode::VerificationFailed, "校验链失败",
                                      plan->template_id));
  }
  return report;
}

Result<std::vector<std::tuple<std::string, std::string, std::string>>> Facade::list_ledgers() {
  std::vector<std::tuple<std::string, std::string, std::string>> out;
  const std::string runtime_root = config_.dirs.root() + "/runtime";
  if (!fs::is_dir(runtime_root)) {
    return out;
  }
  // runtime/<template>/<channel>/<family>/<gpu>/ledger/
  std::error_code ec;
  for (const auto& tmpl_entry : std::filesystem::directory_iterator(runtime_root, ec)) {
    if (!tmpl_entry.is_directory()) {
      continue;
    }
    for (const auto& ch_entry : std::filesystem::directory_iterator(tmpl_entry.path(), ec)) {
      for (const auto& fam_entry : std::filesystem::directory_iterator(ch_entry.path(), ec)) {
        for (const auto& gpu_entry : std::filesystem::directory_iterator(fam_entry.path(), ec)) {
          const auto ids = LedgerStore::list(gpu_entry.path().string() + "/ledger");
          if (!ids) {
            continue;
          }
          const std::string target = ch_entry.path().filename().string() + ":" +
                                     gpu_entry.path().filename().string() + ":" +
                                     fam_entry.path().filename().string();
          for (const auto& id : *ids) {
            out.emplace_back(tmpl_entry.path().filename().string(), target, id);
          }
        }
      }
    }
  }
  return out;
}

Result<std::optional<LedgerEntry>> Facade::latest_ledger(const std::string& ref,
                                                         const BuildTarget& target) {
  auto plan = preview(ref, target);
  if (!plan) {
    return tl::unexpected(std::move(plan.error()));
  }
  auto ids = LedgerStore::list(config_.dirs.ledger_dir(plan->template_id, target));
  if (!ids) {
    return tl::unexpected(std::move(ids.error()));
  }
  if (ids->empty()) {
    return std::nullopt;
  }
  return LedgerStore::load(config_.dirs.ledger_dir(plan->template_id, target), ids->back());
}

// ---------- 用户模板 / 插件 ----------

Result<void> Facade::ut_check(const std::string& path) {
  auto bytes = fs::read_file(path);
  if (!bytes) {
    return tl::unexpected(std::move(bytes.error()));
  }
  auto tpl = TemplateParser::parse(*bytes, "usertemplate:" + path);
  if (!tpl) {
    return tl::unexpected(std::move(tpl.error()));
  }
  return TemplateValidator::validate(*tpl);
}

Result<UserTemplateMeta> Facade::ut_heartbeat(const std::string& id) {
  auto registry = UserTemplateRegistry::load(config_.dirs.user_templates_dir());
  if (!registry) {
    return tl::unexpected(std::move(registry.error()));
  }
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  gmtime_r(&now, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return registry->heartbeat(id, buf);
}

Result<std::vector<UserTemplateMeta>> Facade::ut_list(bool include_archived) {
  auto registry = UserTemplateRegistry::load(config_.dirs.user_templates_dir());
  if (!registry) {
    return tl::unexpected(std::move(registry.error()));
  }
  return registry->list(include_archived);
}

Result<std::vector<std::pair<std::string, std::string>>> Facade::plugin_list() {
  std::vector<std::pair<std::string, std::string>> out;
  const std::string root = config_.dirs.plugins_dir();
  if (!fs::is_dir(root)) {
    return out;
  }
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (!entry.is_directory()) {
      continue;
    }
    const std::string manifest_path = entry.path().string() + "/plugin.toml";
    auto bytes = fs::read_file(manifest_path);
    if (!bytes) {
      continue;
    }
    auto spec = PluginManifestParser::parse(*bytes, "plugin:" + entry.path().filename().string());
    if (!spec) {
      continue;
    }
    out.emplace_back(spec->id + "@" + spec->version, hook_point_str(spec->hook));
  }
  return out;
}

// ---------- 环境自检 ----------

DoctorReport Facade::doctor() {
  DoctorReport report;

  report.checks.push_back({"数据目录可写", fs::exists(config_.dirs.root()),
                           config_.dirs.root()});

  const auto idx = local_index();
  report.checks.push_back(
      {"本地仓库索引", idx.has_value(), idx ? "已 sync（" + std::to_string(idx->templates.size()) +
                                                  " 个模板）"
                                            : idx.error().message});

  const std::string pubkey = config_.sig_pubkey_file.empty()
                                 ? config_.dirs.default_pubkey_path()
                                 : config_.sig_pubkey_file;
  const bool has_pubkey = fs::exists(pubkey);
  report.checks.push_back(
      {"签名公钥", has_pubkey || config_.allow_unsigned_index,
       has_pubkey ? pubkey : (config_.allow_unsigned_index ? "allow_unsigned_index=true（测试模式）"
                                                           : "未找到公钥: " + pubkey)});

  if (!config_.repo_urls.empty()) {
    const auto probe = net::get(config_.repo_urls.front() + "/repo.db");
    report.checks.push_back({"仓库连通", probe.has_value(),
                             probe ? config_.repo_urls.front() : probe.error().message});
  }

  const bool docker = find_in_path("docker");
  const bool podman = find_in_path("podman");
  report.checks.push_back(
      {"容器引擎", docker || podman,
       docker && podman ? "docker + podman" : (docker ? "docker" : (podman ? "podman" : "未找到"))});

  return report;
}

bool DoctorReport::all_ok() const {
  for (const auto& c : checks) {
    if (!c.ok) {
      return false;
    }
  }
  return true;
}

}  // namespace w2ucem::api

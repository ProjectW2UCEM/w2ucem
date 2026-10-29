#include "w2ucem/build/orchestrator.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>

#include "w2ucem/archive/fallback_resolver.hpp"
#include "w2ucem/build/engine_probe.hpp"
#include "w2ucem/build/image_lock.hpp"
#include "w2ucem/build/ledger.hpp"
#include "w2ucem/build/matrix.hpp"
#include "w2ucem/build/runner.hpp"
#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/distro.hpp"
#include "w2ucem/channel/variant.hpp"
#include "w2ucem/template/evaluator.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/logging.hpp"
#include "w2ucem/util/sha256.hpp"
#include "w2ucem/verify/symtab.hpp"
#include "w2ucem/version.hpp"

namespace w2ucem {

namespace {

std::string now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

}  // namespace

Result<BuildResult> BuildOrchestrator::build(const BuildRequest& req) {
  Logger::instance().info("开始构建 {} (target: {})", req.tpl.id,
                          TargetParser::to_string(req.target));

  // ---------- 1. 求值 ----------
  auto plan = TemplateEvaluator::evaluate(req.tpl, req.target, req.template_hash);
  if (!plan) {
    return tl::unexpected(std::move(plan.error()));
  }

  // ---------- 2. 拉取源（镜像轮询 + Archive 回退） ----------
  std::map<std::string, std::string> source_id_to_sha;
  for (const auto& src : plan->sources) {
    auto path = FallbackSourceResolver::resolve(src, config_.archive_urls,
                                                config_.dirs.sources_cache(),
                                                config_.dirs.archive_blob_dir(), false);
    if (!path) {
      return tl::unexpected(std::move(path.error()));
    }
    source_id_to_sha[src.id] = src.sha256;
    Logger::instance().info("源已校验: {} ({})", src.id, src.sha256.substr(0, 12));
  }

  // ---------- 3. 容器引擎与镜像 digest 锁 ----------
  auto backend = EngineProbe::select(req.engine);
  if (!backend) {
    return tl::unexpected(std::move(backend.error()));
  }
  const auto buildenv_digest = ImageDigestLock::pull_and_verify(
      **backend, req.tpl.buildenv.image, req.tpl.buildenv.digest);
  if (!buildenv_digest) {
    return tl::unexpected(std::move(buildenv_digest.error()));
  }
  Logger::instance().info("构建镜像已锁定: {} @ {}", req.tpl.buildenv.image, *buildenv_digest);

  // ---------- 4. 生成装配 runner（确定性，哈希入账本） ----------
  const bool fake_backend = (*backend)->name() == "fake";
  const std::string runner = RunnerScript::generate_assemble(
      *plan, source_id_to_sha, /*with_symtab_extraction=*/!fake_backend);
  const std::string runner_sha256 = Sha256::hex(Sha256::of_bytes(runner));

  // ---------- 5. 目录布局 ----------
  const std::string output_dir = config_.dirs.runtime_dir(plan->template_id, req.target);
  const std::string workdir = output_dir + "/.build";
  if (auto r = fs::ensure_dir(output_dir); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (auto r = fs::ensure_dir(workdir); !r) {
    return tl::unexpected(std::move(r.error()));
  }

  // 插件 hook：PreAssemble（P5 接入 PluginEngine）
  // PluginEngine::run_hooks(HookPoint::PreAssemble, ...)  ← P5

  // ---------- 6. 容器内装配 ----------
  ContainerSpec assemble_spec;
  assemble_spec.image = req.tpl.buildenv.image + "@" + req.tpl.buildenv.digest;
  assemble_spec.mounts.push_back({config_.dirs.sources_cache(), "/sources", true});
  assemble_spec.mounts.push_back({workdir, "/build", false});
  assemble_spec.mounts.push_back({output_dir, "/out", false});
  assemble_spec.env = {"W2UCEM_TEMPLATE=" + plan->template_id,
                       "W2UCEM_TARGET=" + TargetParser::to_string(req.target)};
  assemble_spec.workdir = "/build";
  assemble_spec.command = {"/bin/sh", "-c", runner};

  Logger::instance().info("容器装配中（{}）…", (*backend)->name());
  auto assemble_result = (*backend)->run(assemble_spec);
  if (!assemble_result) {
    return tl::unexpected(std::move(assemble_result.error()));
  }

  // 插件 hook：PostAssemble（P5）

  // ---------- 6.5 symtab 快照（宿主规范化，哈希入账本） ----------
  // 容器内只跑原始 readelf（runner 脚本已提取到 .w2ucem/symtab-raw/<k>.txt）；
  // 这里用与 verify 共用的 canonicalize_symtab 规范化。
  LedgerEntry entry;  // 提前声明：symtab 段与后续共用
  entry.template_id = plan->template_id;
  entry.template_version = plan->template_version;
  entry.template_hash = plan->template_hash;
  entry.evaluator_version = plan->evaluator_version;
  entry.target = TargetParser::to_string(req.target);

  bool has_symtab_declared = false;
  for (const auto& f : plan->manifest.files) {
    if (!f.symtab.empty()) {
      has_symtab_declared = true;
      break;
    }
  }
  if (has_symtab_declared) {
    if ((*backend)->name() == "fake") {
      // macOS 实验路径：无 readelf，跳过提取（账本 symtabs 留空，verify 放行）
      Logger::instance().warn("fake 后端跳过 symtab 提取（{}）——仅 Linux 容器路径支持",
                              req.tpl.id);
    } else {
      int symtab_index = 0;
      for (const auto& f : plan->manifest.files) {
        if (f.symtab.empty()) {
          continue;
        }
        const std::string raw_path =
            output_dir + "/.w2ucem/symtab-raw/" + std::to_string(symtab_index) + ".txt";
        const auto raw = fs::read_file(raw_path);
        if (!raw) {
          return tl::unexpected(Error::make(
              ErrorCode::VerificationFailed,
              "symtab 原始提取缺失（模板声明了 symtab 但产物不可解析）: " + f.path, raw_path));
        }
        const auto snap = verify::canonicalize_symtab(*raw, f.symtab);
        if (!snap) {
          return tl::unexpected(Error::make(ErrorCode::VerificationFailed,
                                            "symtab 规范化失败: " + f.path, raw_path));
        }
        const std::string snap_path =
            output_dir + "/.w2ucem/symtab/" + std::to_string(symtab_index) + ".snap";
        if (auto r = fs::ensure_dir(output_dir + "/.w2ucem/symtab"); !r) {
          return tl::unexpected(std::move(r.error()));
        }
        if (auto r = fs::atomic_write(snap_path, *snap); !r) {
          return tl::unexpected(std::move(r.error()));
        }
        entry.symtabs[f.path] = Sha256::hex(Sha256::of_bytes(*snap));
        Logger::instance().info("symtab 快照: {} ({})", f.path,
                                entry.symtabs[f.path].substr(0, 12));
        ++symtab_index;
      }
      entry.symtab_tool = "readelf";
    }
    entry.buildenv_ref = req.tpl.buildenv.image + "@" + req.tpl.buildenv.digest;
  }

  // ---------- 7. dep-scan（发行版矩阵容器） ----------
  std::vector<std::string> dep_manifests;
  std::vector<std::string> dep_scan_outputs;  // 容器内输出路径
  std::vector<std::string> dep_scan_families;

  const auto matrix = DistroMatrix::load();
  if (!matrix) {
    return tl::unexpected(std::move(matrix.error()));
  }

  std::vector<DistroFamily> families;
  if (req.full_matrix) {
    for (const auto& [family, cell] : (*matrix)->cells()) {
      (void)cell;
      families.push_back(family);
    }
  } else {
    families.push_back(req.target.distro.family);
  }

  std::vector<std::string> elfs;
  for (const auto& step : plan->steps) {
    if (const auto* dep = std::get_if<DepScanStep>(&step)) {
      elfs = dep->elfs;
    }
  }

  if (!elfs.empty()) {
    for (const auto family : families) {
      const auto cell = (*matrix)->cell(family);
      if (!cell) {
        Logger::instance().warn("矩阵中无 {} 的镜像，跳过 dep-scan", family_str(family));
        if (family != req.target.distro.family) {
          entry.dep_scan_skipped.push_back(family_str(family));
        }
        continue;
      }
      const std::string out_rel = req.full_matrix
                                      ? "dep-manifests/" + std::string(family_str(family)) + ".json"
                                      : "dep-manifest.json";
      const std::string out_in_container = "/out/" + out_rel;
      const std::string dep_runner =
          RunnerScript::generate_dep_scan(elfs, family_str(family), out_in_container);

      if ((*backend)->name() == "fake") {
        Logger::instance().warn("fake 后端跳过 dep-scan（{}）——仅 Linux 容器路径支持", family_str(family));
        continue;
      }

      const auto cell_digest = ImageDigestLock::pull_and_verify(**backend, cell->image, cell->digest);
      if (!cell_digest) {
        // 失败语义：目标族 = 模板声明支持的承诺 → 硬失败；
        // 辅助族（full_matrix）= 扩展侦察 → 跳过并留痕（"跳过即记录"）
        if (family == req.target.distro.family) {
          return tl::unexpected(std::move(cell_digest.error()));
        }
        Logger::instance().warn("矩阵镜像锁定失败，跳过 {} 的 dep-scan（{}）", family_str(family),
                                cell_digest.error().message);
        entry.dep_scan_skipped.push_back(family_str(family));
        continue;
      }

      ContainerSpec dep_spec;
      dep_spec.image = cell->image + "@" + cell->digest;
      dep_spec.mounts.push_back({output_dir, "/out", false});
      dep_spec.command = {"/bin/sh", "-c", dep_runner};

      Logger::instance().info("dep-scan 中（{}）…", family_str(family));
      auto dep_result = (*backend)->run(dep_spec);
      if (!dep_result) {
        if (family == req.target.distro.family) {
          return tl::unexpected(std::move(dep_result.error()));
        }
        Logger::instance().warn("跳过 {} 的 dep-scan（执行失败）", family_str(family));
        entry.dep_scan_skipped.push_back(family_str(family));
        continue;
      }
      dep_manifests.push_back(out_rel);
    }
  }

  // ---------- 8. 校验链 ----------
  BuildResult result;
  result.output_dir = output_dir;
  result.dep_manifests = dep_manifests;

  std::optional<LedgerEntry> prior_ledger;
  if (req.verify_on) {
    // record 模式复建比对：读取同 target 下最近账本（若存在）
    auto existing = LedgerStore::list(config_.dirs.ledger_dir(plan->template_id, req.target));
    if (existing && !existing->empty()) {
      auto last = LedgerStore::load(config_.dirs.ledger_dir(plan->template_id, req.target),
                                    existing->back());
      if (last && *last) {
        prior_ledger = **last;
      }
    }
  }

  if (req.verify_on) {
    auto artifact_report = Verifier::verify_artifacts(output_dir, plan->manifest, prior_ledger);
    auto dep_report = Verifier::verify_dep_manifests(output_dir, dep_manifests);
    artifact_report.checks.insert(artifact_report.checks.end(), dep_report.checks.begin(),
                                  dep_report.checks.end());
    // symtab 复验：重新提取 + 规范化 + 与本次构建快照比对
    if (!entry.symtabs.empty()) {
      std::unique_ptr<verify::SymtabExtractor> extractor;
      if ((*backend)->name() != "fake") {
        extractor = std::make_unique<verify::ContainerSymtabExtractor>(
            **backend, entry.buildenv_ref, output_dir);
      }
      auto symtab_report =
          Verifier::verify_symtabs(output_dir, plan->manifest, std::optional<LedgerEntry>{entry},
                                   extractor.get());
      artifact_report.checks.insert(artifact_report.checks.end(), symtab_report.checks.begin(),
                                    symtab_report.checks.end());
    }
    result.verification = std::move(artifact_report);
    if (!result.verification.all_ok()) {
      Logger::instance().error("校验链失败（{} 项未通过）", [&] {
        int n = 0;
        for (const auto& c : result.verification.checks) {
          if (!c.ok) {
            ++n;
          }
        }
        return n;
      }());
      return tl::unexpected(Error::make(ErrorCode::VerificationFailed, "产物校验链失败",
                                        plan->template_id));
    }
    Logger::instance().info("校验链通过（{} 项检查）", result.verification.checks.size());
  }

  // ---------- 9. 账本 ----------
  // entry 已在 symtab 段初始化（template 系列 + symtabs/symtab_tool/buildenv_ref）
  entry.runner_sha256 = runner_sha256;
  entry.timestamp = now_iso8601();
  entry.success = true;
  for (const auto& src : plan->sources) {
    entry.sources.emplace_back(src.id, src.sha256);
  }
  entry.images.emplace_back(req.tpl.buildenv.image, req.tpl.buildenv.digest);
  entry.dep_manifests = dep_manifests;
  result.dep_manifests = dep_manifests;
  result.skipped_dep_scans = entry.dep_scan_skipped;

  // 产物哈希（record 模式首建 / pin 模式留档）
  for (const auto& f : plan->manifest.files) {
    const auto d = Sha256::of_file(output_dir + "/" + f.path);
    if (d) {
      entry.artifacts[f.path] = Sha256::hex(*d);
    }
  }

  // build_id = sha256(全部输入) 前 16 hex
  {
    std::string inputs = entry.template_hash + "|" + entry.target + "|" + runner_sha256;
    for (const auto& [id, sha] : entry.sources) {
      inputs += "|" + id + ":" + sha;
    }
    for (const auto& [img, digest] : entry.images) {
      inputs += "|" + img + "@" + digest;
    }
    entry.build_id = Sha256::hex(Sha256::of_bytes(inputs)).substr(0, 16);
  }
  result.build_id = entry.build_id;

  if (auto r = LedgerStore::append(config_.dirs.ledger_dir(plan->template_id, req.target), entry);
      !r) {
    return tl::unexpected(std::move(r.error()));
  }
  result.ledger = std::move(entry);
  Logger::instance().info("账本已记录: {}", result.build_id);

  return result;
}

}  // namespace w2ucem

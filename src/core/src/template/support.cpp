// template 模块的小型支撑函数实现。
#include <nlohmann/json.hpp>

#include "w2ucem/channel/target.hpp"
#include "w2ucem/template/assemble.hpp"
#include "w2ucem/template/build_depends.hpp"
#include "w2ucem/template/build_plan.hpp"
#include "w2ucem/template/hooks.hpp"
#include "w2ucem/template/manifest.hpp"

namespace w2ucem {

const char* hook_point_str(HookPoint point) {
  switch (point) {
    case HookPoint::PreFetch: return "pre-fetch";
    case HookPoint::PostFetch: return "post-fetch";
    case HookPoint::PreAssemble: return "pre-assemble";
    case HookPoint::PostAssemble: return "post-assemble";
    case HookPoint::PreBuild: return "pre-build";
    case HookPoint::PostBuild: return "post-build";
    case HookPoint::PostInstall: return "post-install";
  }
  return "?";
}

std::optional<HookPoint> hook_point_from_str(const std::string& s) {
  if (s == "pre-fetch") return HookPoint::PreFetch;
  if (s == "post-fetch") return HookPoint::PostFetch;
  if (s == "pre-assemble") return HookPoint::PreAssemble;
  if (s == "post-assemble") return HookPoint::PostAssemble;
  if (s == "pre-build") return HookPoint::PreBuild;
  if (s == "post-build") return HookPoint::PostBuild;
  if (s == "post-install") return HookPoint::PostInstall;
  return std::nullopt;
}

const char* assemble_step_type(const AssembleStep& step) {
  return std::visit(
      [](const auto& s) -> const char* {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, ExtractStep>) return "extract";
        if constexpr (std::is_same_v<T, PatchStep>) return "patch";
        if constexpr (std::is_same_v<T, DllOverrideStep>) return "dll-override";
        if constexpr (std::is_same_v<T, CopyStep>) return "copy";
        if constexpr (std::is_same_v<T, DepScanStep>) return "dep-scan";
        if constexpr (std::is_same_v<T, ManifestStep>) return "manifest";
      },
      step);
}

const std::vector<std::string>& BuildDepends::for_family(DistroFamily family) const {
  const auto it = per_family.find(family);
  if (it != per_family.end()) {
    return it->second;
  }
  return default_pkgs;
}

bool ManifestSpec::has_record_mode() const {
  for (const auto& f : files) {
    if (f.sha256.empty()) {
      return true;
    }
  }
  return false;
}

std::string BuildPlan::to_json() const {
  nlohmann::json j;
  j["target"] = TargetParser::to_string(target);
  j["template_id"] = template_id;
  j["template_version"] = template_version;
  j["template_hash"] = template_hash;
  j["evaluator_version"] = evaluator_version;

  j["sources"] = nlohmann::json::array();
  for (const auto& s : sources) {
    nlohmann::json sj;
    sj["id"] = s.id;
    sj["urls"] = s.urls;
    sj["sha256"] = s.sha256;
    sj["archive_id"] = s.archive_id;
    j["sources"].push_back(std::move(sj));
  }

  j["build_dep_packages"] = build_dep_packages;

  j["steps"] = nlohmann::json::array();
  for (const auto& st : steps) {
    nlohmann::json sj;
    sj["type"] = assemble_step_type(st);
    std::visit(
        [&](const auto& s) {
          using T = std::decay_t<decltype(s)>;
          if constexpr (std::is_same_v<T, ExtractStep>) {
            sj["source"] = s.source;
            sj["to"] = s.to;
          } else if constexpr (std::is_same_v<T, PatchStep>) {
            sj["patch"] = s.patch;
            sj["dir"] = s.dir;
          } else if constexpr (std::is_same_v<T, DllOverrideStep>) {
            sj["rules"] = s.rules;
            sj["output"] = s.output;
          } else if constexpr (std::is_same_v<T, CopyStep>) {
            sj["from"] = s.from;
            sj["to"] = s.to;
          } else if constexpr (std::is_same_v<T, DepScanStep>) {
            sj["elfs"] = s.elfs;
            sj["output"] = s.output;
          } else if constexpr (std::is_same_v<T, ManifestStep>) {
            sj["required"] = s.required;
          }
        },
        st);
    j["steps"].push_back(std::move(sj));
  }

  j["manifest"] = nlohmann::json{{"files", nlohmann::json::array()}};
  for (const auto& f : manifest.files) {
    nlohmann::json fj;
    fj["path"] = f.path;
    fj["sha256"] = f.sha256;
    if (!f.symtab.empty()) {
      fj["symtab"] = f.symtab;
    }
    j["manifest"]["files"].push_back(std::move(fj));
  }

  return j.dump(2);
}

}  // namespace w2ucem

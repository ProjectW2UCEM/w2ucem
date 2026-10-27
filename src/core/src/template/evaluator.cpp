#include "w2ucem/template/evaluator.hpp"

#include <algorithm>
#include <sstream>

#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/distro.hpp"
#include "w2ucem/channel/variant.hpp"
#include "w2ucem/template/parser.hpp"
#include "w2ucem/template/validator.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/version.hpp"

namespace w2ucem {

namespace {

std::string serialize_toml(const toml::value& v) {
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

// 深合并语义（规范红线）：
//   标量/数组：整体替换（数组不拼接）
//   表：递归合并
//   未提及键：继承 base
toml::value deep_merge(const toml::value& base, const toml::value& overlay) {
  if (base.is_table() && overlay.is_table()) {
    auto result = base;
    auto& table = result.as_table();
    for (const auto& [key, value] : overlay.as_table()) {
      const auto it = table.find(key);
      if (it != table.end() && it->second.is_table() && value.is_table()) {
        it->second = deep_merge(it->second, value);
      } else {
        table[key] = value;
      }
    }
    return result;
  }
  return overlay;
}

bool contains(const std::vector<std::string>& list, const std::string& s) {
  return std::find(list.begin(), list.end(), s) != list.end();
}

}  // namespace

Result<BuildPlan> TemplateEvaluator::evaluate(const Template& tpl, const BuildTarget& target,
                                              const std::string& template_hash) {
  const std::string ctx = "template:" + tpl.id;

  // ---------- 域检查：channel/gpu/family 必须在模板声明域内 ----------
  const std::string ch_str = channel_str(target.channel);
  if (!contains(tpl.meta.channels, ch_str)) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "通道 '" + ch_str + "' 不在模板取值域内", ctx));
  }
  const std::string gpu_str = gpu_variant_str(target.gpu);
  if (!contains(tpl.meta.gpu_variants, gpu_str)) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "GPU 变体 '" + gpu_str + "' 不在模板取值域内", ctx));
  }
  const std::string fam_str = family_str(target.distro.family);
  if (!contains(tpl.meta.distro_families, fam_str)) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "发行版族 '" + fam_str + "' 不在模板取值域内", ctx));
  }

  // ---------- 变体合并：TOML 层深合并后重新解析校验 ----------
  Template effective = tpl;
  if (target.gpu != GpuVariant::Generic) {
    const auto it = tpl.variants.find(gpu_str);
    if (it != tpl.variants.end()) {
      const auto merged_raw = deep_merge(tpl.raw, it->second);
      const std::string variant_ctx = ctx + "[" + gpu_str + "]";
      auto merged = TemplateParser::parse(serialize_toml(merged_raw), variant_ctx);
      if (!merged) {
        return tl::unexpected(std::move(merged.error()));
      }
      if (auto v = TemplateValidator::validate(*merged); !v) {
        return tl::unexpected(std::move(v.error()));
      }
      effective = std::move(*merged);
    }
  }

  // ---------- 通道策略（对合并后的 effective 模板双保险） ----------
  if ((target.channel == Channel::Stable || target.channel == Channel::Major) &&
      effective.manifest.has_record_mode()) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "stable/major 通道禁止 record 模式 manifest", ctx));
  }

  // ---------- 组装 BuildPlan ----------
  BuildPlan plan;
  plan.target = target;
  plan.template_id = effective.id;
  plan.template_version = effective.version;
  plan.template_hash = template_hash;
  plan.evaluator_version = EVALUATOR_VERSION;

  for (const auto& [id, s] : effective.sources) {
    plan.sources.push_back(ResolvedSource{id, s.urls, s.sha256, s.archive_id});
  }
  plan.build_dep_packages = effective.build_depends.for_family(target.distro.family);
  plan.steps = effective.steps;
  plan.manifest = effective.manifest;

  return plan;
}

}  // namespace w2ucem

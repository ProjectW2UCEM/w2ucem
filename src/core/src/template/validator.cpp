#include "w2ucem/template/validator.hpp"

#include <algorithm>

#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/distro.hpp"
#include "w2ucem/channel/variant.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/sha256.hpp"
#include "w2ucem/version.hpp"

namespace w2ucem {

namespace {

bool contains(const std::vector<std::string>& list, const std::string& s) {
  return std::find(list.begin(), list.end(), s) != list.end();
}

}  // namespace

Result<void> TemplateValidator::validate(const Template& tpl) {
  const std::string ctx = "template:" + tpl.id;

  if (tpl.format != TEMPLATE_FORMAT_VERSION) {
    return tl::unexpected(Error::make(ErrorCode::Unsupported,
                                      "模板 format 版本 '" + tpl.format + "' 不受支持（期望 " +
                                          TEMPLATE_FORMAT_VERSION + "）",
                                      ctx));
  }
  if (tpl.id.empty() || tpl.version.empty()) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation, "模板 id/version 不能为空", ctx));
  }
  if (tpl.kind != "assembly") {
    return tl::unexpected(Error::make(
        ErrorCode::Unsupported,
        "模板 kind='" + tpl.kind + "' 不受支持（alpha 仅支持 assembly，compile/hybrid 后置）",
        ctx));
  }

  // ---------- 取值域自身合法性 ----------
  if (tpl.meta.channels.empty() || tpl.meta.gpu_variants.empty() ||
      tpl.meta.distro_families.empty()) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "meta 取值域（channels/gpu_variants/distro_families）不能为空",
                                      ctx));
  }
  for (const auto& ch : tpl.meta.channels) {
    if (!channel_from_str(ch)) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "未知通道值 '" + ch + "'", ctx));
    }
  }
  for (const auto& g : tpl.meta.gpu_variants) {
    if (!gpu_variant_from_str(g)) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "未知 GPU 变体值 '" + g + "'", ctx));
    }
  }
  for (const auto& f : tpl.meta.distro_families) {
    if (!family_from_str(f)) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "未知发行版族值 '" + f + "'", ctx));
    }
  }

  // ---------- buildenv：不可变镜像 + digest 锁 ----------
  if (tpl.buildenv.image.empty()) {
    return tl::unexpected(
        Error::make(ErrorCode::SchemaViolation, "buildenv.image 不能为空", ctx));
  }
  if (!tpl.buildenv.digest.starts_with("sha256:")) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "buildenv.digest 须为 'sha256:<hex>' 形式", ctx));
  }
  if (!Sha256::is_valid_hex(tpl.buildenv.digest.substr(7))) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "buildenv.digest 的哈希部分非法", ctx));
  }

  // ---------- sources / patches：sha256 必填且合法 ----------
  for (const auto& [id, s] : tpl.sources) {
    if (s.urls.empty()) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "源 '" + id + "' 的 urls 为空", ctx));
    }
    if (!Sha256::is_valid_hex(s.sha256)) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "源 '" + id + "' 的 sha256 非法", ctx));
    }
    if (!s.archive_id.empty() && !s.archive_id.starts_with("sha256:")) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "源 '" + id + "' 的 archive_id 须为 'sha256:<hex>'",
                                        ctx));
    }
  }
  for (const auto& p : tpl.patches) {
    if (!Sha256::is_valid_hex(p.sha256)) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "补丁 '" + p.path + "' 的 sha256 非法", ctx));
    }
  }

  // ---------- variant 键域 ----------
  for (const auto& [gpu, overlay] : tpl.variants) {
    (void)overlay;
    if (!contains(tpl.meta.gpu_variants, gpu)) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "variant." + gpu + " 未在 gpu_variants 取值域声明",
                                        ctx));
    }
  }

  // ---------- 通道策略红线：stable/major 禁 record 模式 ----------
  if (tpl.manifest.has_record_mode()) {
    for (const auto& ch : tpl.meta.channels) {
      const auto c = channel_from_str(ch);
      if (c == Channel::Stable || c == Channel::Major) {
        return tl::unexpected(Error::make(
            ErrorCode::SchemaViolation,
            "manifest 含 record 模式条目，但模板声明了 " + ch + " 通道（stable/major 禁 record）",
            ctx));
      }
    }
  }
  for (const auto& f : tpl.manifest.files) {
    if (f.path.empty()) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation, "manifest.files 的 path 不能为空", ctx));
    }
  }

  return {};
}

}  // namespace w2ucem

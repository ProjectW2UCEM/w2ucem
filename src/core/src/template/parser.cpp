#include "w2ucem/template/parser.hpp"

#include <toml.hpp>

#include "w2ucem/template/assemble.hpp"
#include "w2ucem/template/hooks.hpp"
#include "w2ucem/util/error.hpp"

namespace w2ucem {

namespace {

// ---------- 严格 schema 助手 ----------

// 未知键 = 硬错误：拼错键静默忽略在可复现系统里就是构建漂移。
Result<void> check_keys(const toml::value& table, std::initializer_list<const char*> allowed,
                        const std::string& path, const std::string& ctx) {
  for (const auto& [key, value] : table.as_table()) {
    (void)value;
    bool found = false;
    for (const char* a : allowed) {
      if (key == a) {
        found = true;
        break;
      }
    }
    if (!found) {
      return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                        "未知键 '" + key + "'（位置: " + path + "）", ctx));
    }
  }
  return {};
}

Result<std::string> req_str(const toml::value& table, const std::string& key,
                            const std::string& path, const std::string& ctx) {
  try {
    return toml::find<std::string>(table, key);
  } catch (const std::exception& e) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "缺少或类型错误: '" + key + "'（位置: " + path + "）: " +
                                          e.what(),
                                      ctx));
  }
}

std::optional<std::string> opt_str(const toml::value& table, const std::string& key) {
  try {
    return toml::find<std::string>(table, key);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

Result<std::vector<std::string>> req_str_array(const toml::value& table,
                                               const std::string& key,
                                               const std::string& path,
                                               const std::string& ctx) {
  try {
    return toml::find<std::vector<std::string>>(table, key);
  } catch (const std::exception& e) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "缺少或类型错误: '" + key + "'（位置: " + path + "）: " +
                                          e.what(),
                                      ctx));
  }
}

[[nodiscard]] std::string step_path(std::size_t index) {
  return "assemble.steps[" + std::to_string(index) + "]";
}

// ---------- assemble 步骤解析 ----------

Result<AssembleStep> parse_step(const toml::value& step, const std::string& path,
                                const std::string& ctx) {
  if (!step.is_table()) {
    return tl::unexpected(
        Error::make(ErrorCode::SchemaViolation, "步骤必须为表（位置: " + path + "）", ctx));
  }
  const auto type = req_str(step, "type", path, ctx);
  if (!type) {
    return tl::unexpected(std::move(type.error()));
  }

  if (*type == "extract") {
    if (auto r = check_keys(step, {"type", "source", "to"}, path, ctx); !r) return tl::unexpected(std::move(r.error()));
    ExtractStep s;
    auto src = req_str(step, "source", path, ctx);
    auto to = req_str(step, "to", path, ctx);
    if (!src || !to) return tl::unexpected(!src ? src.error() : to.error());
    s.source = *src;
    s.to = *to;
    return s;
  }
  if (*type == "patch") {
    if (auto r = check_keys(step, {"type", "patch", "dir"}, path, ctx); !r) return tl::unexpected(std::move(r.error()));
    PatchStep s;
    auto p = req_str(step, "patch", path, ctx);
    auto d = req_str(step, "dir", path, ctx);
    if (!p || !d) return tl::unexpected(!p ? p.error() : d.error());
    s.patch = *p;
    s.dir = *d;
    return s;
  }
  if (*type == "dll-override") {
    if (auto r = check_keys(step, {"type", "rules", "output"}, path, ctx); !r) return tl::unexpected(std::move(r.error()));
    DllOverrideStep s;
    auto rules = req_str_array(step, "rules", path, ctx);
    auto out = req_str(step, "output", path, ctx);
    if (!rules || !out) return tl::unexpected(!rules ? rules.error() : out.error());
    s.rules = *rules;
    s.output = *out;
    return s;
  }
  if (*type == "copy") {
    if (auto r = check_keys(step, {"type", "from", "to"}, path, ctx); !r) return tl::unexpected(std::move(r.error()));
    CopyStep s;
    auto from = req_str(step, "from", path, ctx);
    auto to = req_str(step, "to", path, ctx);
    if (!from || !to) return tl::unexpected(!from ? from.error() : to.error());
    s.from = *from;
    s.to = *to;
    return s;
  }
  if (*type == "dep-scan") {
    if (auto r = check_keys(step, {"type", "elfs", "output"}, path, ctx); !r) return tl::unexpected(std::move(r.error()));
    DepScanStep s;
    auto elfs = req_str_array(step, "elfs", path, ctx);
    auto out = req_str(step, "output", path, ctx);
    if (!elfs || !out) return tl::unexpected(!elfs ? elfs.error() : out.error());
    s.elfs = *elfs;
    s.output = *out;
    return s;
  }
  if (*type == "manifest") {
    if (auto r = check_keys(step, {"type", "required"}, path, ctx); !r) return tl::unexpected(std::move(r.error()));
    ManifestStep s;
    auto reqd = req_str_array(step, "required", path, ctx);
    if (!reqd) return tl::unexpected(std::move(reqd.error()));
    s.required = *reqd;
    return s;
  }
  return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                    "未知步骤类型 '" + *type + "'（位置: " + path + "）", ctx));
}

}  // namespace

Result<Template> TemplateParser::parse(const std::string& toml_bytes, const std::string& context) {
  toml::value root;
  try {
    root = toml::parse_str(toml_bytes);
  } catch (const toml::syntax_error& e) {
    return tl::unexpected(Error::make(ErrorCode::Parse, e.what(), context));
  }

  Template tpl;
  tpl.raw = root;

  // ---------- 顶层键 ----------
  if (auto r = check_keys(root,
                          {"template", "meta", "buildenv", "sources", "patches",
                           "build_depends", "assemble", "variant", "hooks", "manifest"},
                          "<root>", context);
      !r) {
    return tl::unexpected(std::move(r.error()));
  }

  // ---------- [template] ----------
  if (!root.contains("template")) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation, "缺少 [template] 表", context));
  }
  const auto& t = root.at("template");
  if (auto r = check_keys(t, {"format", "id", "name", "version", "kind", "upstream"},
                          "template", context);
      !r) {
    return tl::unexpected(std::move(r.error()));
  }
  for (const auto& key : {"format", "id", "version", "kind"}) {
    const auto v = req_str(t, key, "template", context);
    if (!v) {
      return tl::unexpected(std::move(v.error()));
    }
  }
  tpl.format = *req_str(t, "format", "template", context);
  tpl.id = *req_str(t, "id", "template", context);
  tpl.version = *req_str(t, "version", "template", context);
  tpl.kind = *req_str(t, "kind", "template", context);
  tpl.name = opt_str(t, "name").value_or(tpl.id);
  tpl.upstream = opt_str(t, "upstream").value_or("");

  // ---------- [meta] ----------
  if (!root.contains("meta")) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation, "缺少 [meta] 表", context));
  }
  const auto& m = root.at("meta");
  if (auto r = check_keys(m, {"maintainer", "license", "channels", "gpu_variants",
                              "distro_families"},
                          "meta", context);
      !r) {
    return tl::unexpected(std::move(r.error()));
  }
  {
    const auto v = req_str_array(m, "channels", "meta", context);
    if (!v) return tl::unexpected(std::move(v.error()));
    tpl.meta.channels = *v;
  }
  {
    const auto v = req_str_array(m, "gpu_variants", "meta", context);
    if (!v) return tl::unexpected(std::move(v.error()));
    tpl.meta.gpu_variants = *v;
  }
  {
    const auto v = req_str_array(m, "distro_families", "meta", context);
    if (!v) return tl::unexpected(std::move(v.error()));
    tpl.meta.distro_families = *v;
  }
  tpl.meta.maintainer = opt_str(m, "maintainer").value_or("");
  tpl.meta.license = opt_str(m, "license").value_or("");

  // ---------- [buildenv] ----------
  if (!root.contains("buildenv")) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation, "缺少 [buildenv] 表", context));
  }
  const auto& be = root.at("buildenv");
  if (auto r = check_keys(be, {"image", "digest"}, "buildenv", context); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  {
    const auto v = req_str(be, "image", "buildenv", context);
    if (!v) return tl::unexpected(std::move(v.error()));
    tpl.buildenv.image = *v;
  }
  {
    const auto v = req_str(be, "digest", "buildenv", context);
    if (!v) return tl::unexpected(std::move(v.error()));
    tpl.buildenv.digest = *v;
  }

  // ---------- [sources.*] ----------
  if (root.contains("sources")) {
    const auto& src = root.at("sources");
    for (const auto& [id, v] : src.as_table()) {
      const std::string path = "sources." + id;
      if (!v.is_table()) {
        return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                          "源必须为表（位置: " + path + "）", context));
      }
      if (auto r = check_keys(v, {"urls", "sha256", "archive_id"}, path, context); !r) {
        return tl::unexpected(std::move(r.error()));
      }
      SourceSpec spec;
      spec.id = id;
      {
        const auto urls = req_str_array(v, "urls", path, context);
        if (!urls) return tl::unexpected(std::move(urls.error()));
        spec.urls = *urls;
      }
      {
        const auto sha = req_str(v, "sha256", path, context);
        if (!sha) return tl::unexpected(std::move(sha.error()));
        spec.sha256 = *sha;
      }
      spec.archive_id = opt_str(v, "archive_id").value_or("");
      tpl.sources[id] = std::move(spec);
    }
  }

  // ---------- [[patches]] ----------
  if (root.contains("patches")) {
    const auto patches = toml::find<std::vector<toml::value>>(root, "patches");
    for (std::size_t i = 0; i < patches.size(); ++i) {
      const std::string path = "patches[" + std::to_string(i) + "]";
      if (auto r = check_keys(patches[i], {"path", "sha256"}, path, context); !r) {
        return tl::unexpected(std::move(r.error()));
      }
      PatchSpec p;
      {
        const auto v = req_str(patches[i], "path", path, context);
        if (!v) return tl::unexpected(std::move(v.error()));
        p.path = *v;
      }
      {
        const auto v = req_str(patches[i], "sha256", path, context);
        if (!v) return tl::unexpected(std::move(v.error()));
        p.sha256 = *v;
      }
      tpl.patches.push_back(std::move(p));
    }
  }

  // ---------- [build_depends] ----------
  if (root.contains("build_depends")) {
    const auto& bd = root.at("build_depends");
    if (auto r = check_keys(bd, {"default", "arch", "deb", "rpm", "alpine"}, "build_depends",
                            context);
        !r) {
      return tl::unexpected(std::move(r.error()));
    }
    try {
      if (bd.contains("default")) {
        tpl.build_depends.default_pkgs = toml::find<std::vector<std::string>>(bd, "default");
      }
      if (bd.contains("arch")) {
        tpl.build_depends.per_family[DistroFamily::Arch] =
            toml::find<std::vector<std::string>>(bd, "arch");
      }
      if (bd.contains("deb")) {
        tpl.build_depends.per_family[DistroFamily::Debian] =
            toml::find<std::vector<std::string>>(bd, "deb");
      }
      if (bd.contains("rpm")) {
        tpl.build_depends.per_family[DistroFamily::Rpm] =
            toml::find<std::vector<std::string>>(bd, "rpm");
      }
      if (bd.contains("alpine")) {
        tpl.build_depends.per_family[DistroFamily::Alpine] =
            toml::find<std::vector<std::string>>(bd, "alpine");
      }
    } catch (const std::exception& e) {
      return tl::unexpected(
          Error::make(ErrorCode::SchemaViolation,
                      "build_depends 条目类型错误: " + std::string(e.what()), context));
    }
  }

  // ---------- [[assemble.steps]] ----------
  if (root.contains("assemble")) {
    const auto& asm_tbl = root.at("assemble");
    if (auto r = check_keys(asm_tbl, {"steps"}, "assemble", context); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    if (asm_tbl.contains("steps")) {
      try {
        const auto steps = toml::find<std::vector<toml::value>>(asm_tbl, "steps");
        for (std::size_t i = 0; i < steps.size(); ++i) {
          auto step = parse_step(steps[i], step_path(i), context);
          if (!step) {
            return tl::unexpected(std::move(step.error()));
          }
          tpl.steps.push_back(*step);
        }
      } catch (const std::exception& e) {
        return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                          "assemble.steps 解析失败: " + std::string(e.what()),
                                          context));
      }
    }
  }

  // ---------- [variant.*] ----------
  if (root.contains("variant")) {
    const auto& var = root.at("variant");
    for (const auto& [gpu, overlay] : var.as_table()) {
      if (!overlay.is_table()) {
        return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                          "variant." + gpu + " 必须为表", context));
      }
      tpl.variants[gpu] = overlay;
    }
  }

  // ---------- [hooks] ----------
  if (root.contains("hooks")) {
    const auto& hk = root.at("hooks");
    if (auto r = check_keys(hk,
                            {"pre-fetch", "post-fetch", "pre-assemble", "post-assemble",
                             "pre-build", "post-build", "post-install"},
                            "hooks", context);
        !r) {
      return tl::unexpected(std::move(r.error()));
    }
    for (const auto& [key, v] : hk.as_table()) {
      if (auto r = check_keys(v, {"plugins"}, "hooks." + key, context); !r) {
        return tl::unexpected(std::move(r.error()));
      }
      const auto point = hook_point_from_str(key);
      const auto plugins = toml::find<std::vector<std::string>>(v, "plugins");
      tpl.hooks.plugins[*point] = plugins;
    }
  }

  // ---------- [manifest] ----------
  if (root.contains("manifest")) {
    const auto& mf = root.at("manifest");
    if (auto r = check_keys(mf, {"files"}, "manifest", context); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    if (mf.contains("files")) {
      try {
        const auto files = toml::find<std::vector<toml::value>>(mf, "files");
        for (std::size_t i = 0; i < files.size(); ++i) {
          const std::string path = "manifest.files[" + std::to_string(i) + "]";
          if (auto r = check_keys(files[i], {"path", "sha256", "symtab"}, path, context); !r) {
            return tl::unexpected(std::move(r.error()));
          }
          ManifestFileSpec f;
          {
            const auto v = req_str(files[i], "path", path, context);
            if (!v) return tl::unexpected(std::move(v.error()));
            f.path = *v;
          }
          f.sha256 = opt_str(files[i], "sha256").value_or("");
          f.symtab = opt_str(files[i], "symtab").value_or("");
          tpl.manifest.files.push_back(std::move(f));
        }
      } catch (const std::exception& e) {
        return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                          "manifest.files 解析失败: " + std::string(e.what()),
                                          context));
      }
    }
  }

  return tpl;
}

}  // namespace w2ucem

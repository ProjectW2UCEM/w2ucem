#include "w2ucem/build/runner.hpp"

#include <nlohmann/json.hpp>

#include <sstream>

#include "w2ucem/template/assemble.hpp"

namespace w2ucem {

namespace {

// 路径变量替换（容器内约定）：$build → /build，$prefix → /out
std::string expand(const std::string& path) {
  std::string out = path;
  const auto replace_all = [&out](const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = out.find(from, pos)) != std::string::npos) {
      out.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replace_all("$build", "/build");
  replace_all("$prefix", "/out");
  return out;
}

// 相对产物根的路径：dll-override / dep-scan 的 output 字段约定相对 $prefix。
// 带 $build/$prefix 变量的路径先展开，纯相对路径补 /out 前缀。
std::string prefix_relative(const std::string& path) {
  const std::string expanded = expand(path);
  if (!expanded.empty() && expanded[0] == '/') {
    return expanded;
  }
  return "/out/" + expanded;
}

// shell 单引号转义（symtab 提取路径防注入）。
std::string shell_quote(const std::string& s) {
  std::string out = "'";
  for (const char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += '\'';
  return out;
}

}  // namespace

std::string RunnerScript::generate_assemble(
    const BuildPlan& plan, const std::map<std::string, std::string>& source_id_to_sha256,
    bool with_symtab_extraction) {
  std::ostringstream s;
  s << "#!/bin/sh\n"
    << "# W2UCEM 确定性装配脚本（由 RunnerScript 生成，sha256 记入账本）\n"
    << "set -eu\n"
    << "export LC_ALL=C\n"
    << "mkdir -p /build /out\n\n";

  for (const auto& step : plan.steps) {
    std::visit(
        [&](const auto& st) {
          using T = std::decay_t<decltype(st)>;
          if constexpr (std::is_same_v<T, ExtractStep>) {
            const auto it = source_id_to_sha256.find(st.source);
            s << "# extract " << st.source << "\n";
            s << "mkdir -p " << expand(st.to) << "\n";
            if (it != source_id_to_sha256.end()) {
              s << "tar -xzf /sources/" << it->second << " -C " << expand(st.to) << "\n";
            } else {
              s << "echo \"[w2ucem] 源 " << st.source << " 未在计划中\" >&2\nexit 1\n";
            }
          } else if constexpr (std::is_same_v<T, PatchStep>) {
            s << "# patch " << st.patch << "\n";
            s << "patch -p1 -d " << expand(st.dir) << " < " << expand(st.patch) << "\n";
          } else if constexpr (std::is_same_v<T, DllOverrideStep>) {
            // 确定性生成 override 清单
            nlohmann::json j = nlohmann::json{{"dll_overrides", nlohmann::json::object()}};
            for (const auto& rule : st.rules) {
              const auto eq = rule.find('=');
              if (eq != std::string::npos) {
                j["dll_overrides"][rule.substr(0, eq)] = rule.substr(eq + 1);
              }
            }
            s << "# dll-override → " << prefix_relative(st.output) << "\n";
            s << "cat > " << prefix_relative(st.output) << " <<'W2UCEM_EOF'\n";
            s << j.dump(2) << "\n";
            s << "W2UCEM_EOF\n";
          } else if constexpr (std::is_same_v<T, CopyStep>) {
            // 语义：把 from 整体放到 to 下（保留 basename，如 files → $prefix/files）
            s << "# copy " << st.from << " → " << st.to << "\n";
            s << "mkdir -p " << expand(st.to) << "\n";
            s << "cp -r " << expand(st.from) << " " << expand(st.to) << "/\n";
          } else if constexpr (std::is_same_v<T, DepScanStep>) {
            // dep-scan 在矩阵容器内执行，装配脚本跳过（占位提示）
            s << "# dep-scan 将在发行版矩阵容器内执行（输出 " << st.output << "）\n";
          } else if constexpr (std::is_same_v<T, ManifestStep>) {
            s << "# manifest 断言\n";
            for (const auto& req : st.required) {
              s << "test -e " << prefix_relative(req) << " || { echo \"[w2ucem] 缺失产物: "
                << req << "\" >&2; exit 1; }\n";
            }
          }
        },
        step);
    s << "\n";
  }

  // ---------- symtab 原始提取（宿主规范化） ----------
  // 对 manifest 中声明 symtab 的条目：容器内只跑一条原始 readelf 命令，
  // 原始输出落 .w2ucem/symtab-raw/<k>.txt，规范化由宿主 C++ 完成。
  // fake 后端（with_symtab_extraction=false）不生成——宿主无 readelf。
  if (with_symtab_extraction) {
    int symtab_index = 0;
    for (const auto& f : plan.manifest.files) {
      if (f.symtab.empty()) {
        continue;
      }
      s << "# symtab 原始提取: " << f.path << "\n";
      s << "mkdir -p /out/.w2ucem/symtab-raw\n";
      s << "readelf -Ws " << shell_quote(prefix_relative(f.path))
        << " > /out/.w2ucem/symtab-raw/" << symtab_index << ".txt 2>/dev/null || { "
        << "echo \"[w2ucem] symtab 提取失败: " << f.path << "\" >&2; exit 1; }\n";
      ++symtab_index;
    }
  }

  return s.str();
}

std::string RunnerScript::generate_dep_scan(const std::vector<std::string>& elfs,
                                            const std::string& family,
                                            const std::string& output_path) {
  std::ostringstream s;
  s << "#!/bin/sh\n"
    << "# W2UCEM dep-scan 脚本（矩阵容器内：readelf 解析 DT_NEEDED → 依赖清单）\n"
    << "set -eu\n"
    << "export LC_ALL=C\n"
    << "OUT=" << output_path << "\n"
    << "mkdir -p \"$(dirname \"$OUT\")\"\n"
    << "{\n"
    << "  printf '{\"schema\":1,\"family\":\"" << family << "\",\"elfs\":['\n"
    << "  first=1\n";
  for (const auto& elf : elfs) {
    s << "  [ \"$first\" = 0 ] && printf ','\n"
      << "  first=0\n"
      << "  readelf -d " << expand(elf)
      << " 2>/dev/null | sed -n 's/.*(NEEDED).*Shared library: \\[\\(.*\\)\\].*/\\1/p' > /tmp/w2ucem-needed.$$ || true\n"
      << "  printf '{\"path\":\"" << expand(elf) << "\",\"needed\":['\n"
      << "  nf=1\n"
      << "  while IFS= read -r lib; do\n"
      << "    [ -z \"$lib\" ] && continue\n"
      << "    [ \"$nf\" = 0 ] && printf ','\n"
      << "    nf=0\n"
      << "    printf '\"%s\"' \"$lib\"\n"
      << "  done < /tmp/w2ucem-needed.$$\n"
      << "  rm -f /tmp/w2ucem-needed.$$\n"
      << "  printf '],\"packages\":[]}'\n";
  }
  s << "  printf ']}\\n'\n"
    << "} > \"$OUT\"\n";
  return s.str();
}

}  // namespace w2ucem

#include "w2ucem/verify/symtab.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "w2ucem/build/backend.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/logging.hpp"
#include "w2ucem/util/process.hpp"

namespace w2ucem::verify {

namespace {

// shell 单引号转义：' → '\''
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

// Ndx 归一化：UND/ABS 保留（导入面敏感），数字节索引 → DEF（链接布局函数）。
std::string normalize_ndx(const std::string& ndx) {
  if (ndx == "UND" || ndx == "ABS") {
    return ndx;
  }
  return "DEF";
}

std::vector<std::string> split_ws(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

}  // namespace

Result<std::string> canonicalize_symtab(const std::string& readelf_raw,
                                        const std::string& scope) {
  if (scope != "dyn" && scope != "full") {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "symtab scope 须为 dyn|full（当前: '" + scope + "'）"));
  }

  std::vector<std::string> symbol_lines;
  std::string current_section;
  bool in_dynsym = false;
  bool in_symtab = false;

  std::istringstream input(readelf_raw);
  std::string line;
  while (std::getline(input, line)) {
    // 节头跟踪
    if (line.find("Symbol table '") != std::string::npos) {
      current_section = line;
      in_dynsym = line.find("'.dynsym'") != std::string::npos;
      in_symtab = line.find("'.symtab'") != std::string::npos;
      continue;
    }
    // scope 过滤：dyn 只收 .dynsym；full 收两节
    if (!in_dynsym && !in_symtab) {
      continue;
    }
    if (scope == "dyn" && !in_dynsym) {
      continue;
    }
    // 跳过表头/空行
    if (line.find("Num:") != std::string::npos) {
      continue;
    }
    const auto tokens = split_ws(line);
    // 符号行：Num: Value Size Type Bind Vis Ndx [Name]
    if (tokens.size() < 7 || tokens[0].find(':') == std::string::npos) {
      continue;
    }
    const std::string& type = tokens[3];
    const std::string& bind = tokens[4];
    const std::string& vis = tokens[5];
    const std::string ndx = normalize_ndx(tokens[6]);
    const std::string name = tokens.size() > 7 ? tokens[7] : "";
    symbol_lines.push_back(bind + " " + type + " " + vis + " " + ndx + " " + name);
  }

  // 集合语义：排序 + 去重（输出是输入行集的可交换纯函数）
  std::sort(symbol_lines.begin(), symbol_lines.end());
  symbol_lines.erase(std::unique(symbol_lines.begin(), symbol_lines.end()), symbol_lines.end());

  std::ostringstream snapshot;
  snapshot << "w2ucem-symtab v1\n";
  snapshot << "tool: readelf\n";
  snapshot << "scope: " << scope << "\n";
  for (const auto& s : symbol_lines) {
    snapshot << s << "\n";
  }
  return snapshot.str();
}

// ---------- ContainerSymtabExtractor ----------

ContainerSymtabExtractor::ContainerSymtabExtractor(ContainerBackend& backend,
                                                   const std::string& image_ref,
                                                   const std::string& output_dir)
    : backend_(backend), image_ref_(image_ref), output_dir_(output_dir) {}

Result<std::string> ContainerSymtabExtractor::extract_raw(
    const std::string& artifact_rel_path) {
  ContainerSpec spec;
  spec.image = image_ref_;
  spec.mounts.push_back({output_dir_, "/out", false});
  const std::string cmd =
      "readelf -Ws " + shell_quote("/out/" + artifact_rel_path) + " 2>/dev/null";
  spec.command = {"/bin/sh", "-c", cmd};

  auto r = backend_.run(spec);
  if (!r) {
    return tl::unexpected(std::move(r.error()));
  }
  return r->log;
}

// ---------- HostSymtabExtractor ----------

HostSymtabExtractor::HostSymtabExtractor() {
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return;
  }
  std::stringstream ss(path_env);
  std::string dir;
  while (std::getline(ss, dir, ':')) {
    const std::string candidate = dir + "/readelf";
    if (fs::exists(candidate)) {
      readelf_path_ = candidate;
      return;
    }
  }
}

Result<std::string> HostSymtabExtractor::extract_raw(const std::string& artifact_rel_path) {
  if (readelf_path_.empty()) {
    return tl::unexpected(Error::make(ErrorCode::Unsupported, "宿主无 readelf（macOS 实验路径）",
                                      artifact_rel_path));
  }
  auto r = process::run({readelf_path_, "-Ws", artifact_rel_path}, "host readelf");
  if (!r) {
    return tl::unexpected(std::move(r.error()));
  }
  if (r->exit_code != 0) {
    return tl::unexpected(Error::make(ErrorCode::ContainerFailure,
                                      "readelf 提取失败: " + r->output, artifact_rel_path));
  }
  return r->output;
}

// ---------- SymtabChecker ----------

std::string SymtabChecker::snapshot(const std::string& artifact_rel_path,
                                    const std::string& scope) {
  if (extractor_ == nullptr) {
    return "";
  }
  auto raw = extractor_->extract_raw(artifact_rel_path);
  if (!raw) {
    Logger::instance().warn("符号表提取失败（{}）: {}", artifact_rel_path, raw.error().message);
    return "";
  }
  auto snap = canonicalize_symtab(*raw, scope);
  if (!snap) {
    Logger::instance().warn("符号表规范化失败（{}）: {}", artifact_rel_path, snap.error().message);
    return "";
  }
  return *snap;
}

}  // namespace w2ucem::verify

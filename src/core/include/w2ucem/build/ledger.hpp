// Ledger：可复现性账本——"源头→用户"的证据链。
// 每次构建记录全部输入哈希（模板/源/镜像/插件/runner）与产物哈希，
// 任何 verify 都能回答"每个输入字节从哪来、hash 是什么"。
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct LedgerEntry {
  std::string build_id;
  std::string template_id;
  std::string template_version;
  std::string template_hash;
  std::string evaluator_version;
  std::string target;
  std::vector<std::pair<std::string, std::string>> sources;  // id → sha256
  std::vector<std::pair<std::string, std::string>> images;   // image → digest
  std::vector<std::string> plugin_hashes;
  std::string runner_sha256;
  std::map<std::string, std::string> artifacts;  // 产物相对路径 → sha256
  std::map<std::string, std::string> symtabs;    // 产物相对路径 → 规范化快照 sha256
  std::string symtab_tool;                       // "readelf"；空 = 未启用
  std::string buildenv_ref;                      // "image@digest"：verify 复验的提取环境
  std::vector<std::string> dep_manifests;
  std::vector<std::string> dep_scan_skipped;  // 跳过的矩阵 family（镜像锁定失败）
  std::string timestamp;
  bool success{true};

  [[nodiscard]] std::string to_json() const;
  [[nodiscard]] static Result<LedgerEntry> from_json(const std::string& json,
                                                     const std::string& context);
};

struct LedgerStore {
  // 原子写 <dir>/<build_id>.json + 追加 <dir>/index.json（记录 build_id → 路径）。
  [[nodiscard]] static Result<void> append(const std::string& dir, const LedgerEntry& entry);

  [[nodiscard]] static Result<std::optional<LedgerEntry>> load(const std::string& dir,
                                                               const std::string& build_id);

  [[nodiscard]] static Result<std::vector<std::string>> list(const std::string& dir);
};

}  // namespace w2ucem

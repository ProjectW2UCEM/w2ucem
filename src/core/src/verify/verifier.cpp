#include "w2ucem/verify/verifier.hpp"

#include <nlohmann/json.hpp>

#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/sha256.hpp"

namespace w2ucem {

bool VerificationReport::all_ok() const {
  for (const auto& c : checks) {
    if (!c.ok) {
      return false;
    }
  }
  return true;
}

std::string SymtabChecker::snapshot(const std::string& elf_path) {
  (void)elf_path;
  return "";  // alpha 未启用
}

VerificationReport Verifier::verify_artifacts(const std::string& output_dir,
                                              const ManifestSpec& manifest,
                                              const std::optional<LedgerEntry>& ledger) {
  VerificationReport report;

  for (const auto& f : manifest.files) {
    const std::string full = output_dir + "/" + f.path;

    const bool exists = fs::exists(full);
    report.checks.push_back({"存在: " + f.path, exists, full});
    if (!exists) {
      continue;
    }

    const auto actual = Sha256::of_file(full);
    if (!actual) {
      report.checks.push_back({"哈希: " + f.path, false, actual.error().message});
      continue;
    }
    const std::string actual_hex = Sha256::hex(*actual);

    if (!f.sha256.empty()) {
      // pin 模式：与声明值比对
      const bool ok = actual_hex == f.sha256;
      report.checks.push_back(
          {"pin 校验: " + f.path, ok, ok ? actual_hex : "期望 " + f.sha256 + " 实际 " + actual_hex});
    } else {
      // record 模式：与账本首建值比对
      if (ledger) {
        const auto it = ledger->artifacts.find(f.path);
        if (it == ledger->artifacts.end()) {
          report.checks.push_back(
              {"record 校验: " + f.path, false, "账本中无此产物的首建记录"});
        } else {
          const bool ok = it->second == actual_hex;
          report.checks.push_back(
              {"record 校验: " + f.path, ok,
               ok ? actual_hex : "账本记录 " + it->second + " 实际 " + actual_hex});
        }
      } else {
        report.checks.push_back(
            {"record 校验: " + f.path, true, actual_hex + "（首建，无账本可比对）"});
      }
    }
  }

  return report;
}

VerificationReport Verifier::verify_dep_manifests(const std::string& output_dir,
                                                  const std::vector<std::string>& relative_paths) {
  VerificationReport report;
  for (const auto& rel : relative_paths) {
    const std::string full = output_dir + "/" + rel;
    const bool exists = fs::exists(full);
    report.checks.push_back({"存在: " + rel, exists, full});
    if (!exists) {
      continue;
    }
    auto bytes = fs::read_file(full);
    if (!bytes) {
      report.checks.push_back({"解析: " + rel, false, bytes.error().message});
      continue;
    }
    try {
      const auto j = nlohmann::json::parse(*bytes);
      const bool schema_ok = j.contains("schema") && j.at("schema").get<int>() == 1 &&
                             j.contains("family") && j.contains("elfs") &&
                             j.at("elfs").is_array();
      report.checks.push_back({"schema: " + rel, schema_ok, schema_ok ? "v1" : "字段缺失"});
    } catch (const std::exception& e) {
      report.checks.push_back({"schema: " + rel, false, e.what()});
    }
  }
  return report;
}

}  // namespace w2ucem

// 符号表规范化器与账本扩展单测（golden 驱动，macOS 无需 readelf）。
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "w2ucem/build/ledger.hpp"
#include "w2ucem/template/parser.hpp"
#include "w2ucem/template/validator.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/verify/symtab.hpp"

using namespace w2ucem;

namespace {

std::string fixture(const std::string& name) {
  return std::string(TEST_SOURCE_DIR) + "/../fixtures/elf/" + name;
}

// 合法模板 + 注入 symtab 字段
std::string template_with_symtab(const std::string& symtab_value) {
  return R"(
[template]
format = "0.1"
id = "symtest"
version = "1"
kind = "assembly"

[meta]
channels = ["semi-rolling"]
gpu_variants = ["generic"]
distro_families = ["deb"]

[buildenv]
image = "w2ucem/buildenv-alpine"
digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000"

[manifest]
[[manifest.files]]
path = "files/bin/mockelf"
sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
symtab = ")" +
         symtab_value + R"("
)";
}

}  // namespace

TEST_CASE("symtab 规范化：golden full 逐字节比对") {
  const auto raw = fs::read_file(fixture("mockelf.readelf-Ws.txt"));
  REQUIRE(raw);
  const auto golden = fs::read_file(fixture("mockelf.canonical.txt"));
  REQUIRE(golden);

  const auto snapshot = w2ucem::verify::canonicalize_symtab(*raw, "full");
  REQUIRE(snapshot);
  INFO("实际快照:\n" << *snapshot << "\n=== 期待 golden:\n" << *golden);
  REQUIRE(*snapshot == *golden);
}

TEST_CASE("symtab 规范化：dyn 是 full 的真子集") {
  const auto raw = fs::read_file(fixture("mockelf.readelf-Ws.txt"));
  REQUIRE(raw);

  const auto full = w2ucem::verify::canonicalize_symtab(*raw, "full");
  const auto dyn = w2ucem::verify::canonicalize_symtab(*raw, "dyn");
  REQUIRE(full);
  REQUIRE(dyn);

  // dyn 只含 .dynsym 条目：internal_helper 是 .symtab LOCAL 符号，不得出现
  REQUIRE(dyn->find("internal_helper") == std::string::npos);
  REQUIRE(full->find("internal_helper") != std::string::npos);
  // 导入面在两 scope 都有
  REQUIRE(dyn->find("puts@GLIBC_2.2.5") != std::string::npos);
  // 每一行（跳过头部 3 行）都是 full 的行
  for (const auto& line : [&] {
         std::vector<std::string> lines;
         std::istringstream iss(*dyn);
         std::string l;
         std::getline(iss, l);  // w2ucem-symtab v1
         std::getline(iss, l);  // tool:
         std::getline(iss, l);  // scope:
         while (std::getline(iss, l)) {
           lines.push_back(l);
         }
         return lines;
       }()) {
    const bool in_full =
        full->find(line + "\n") != std::string::npos || full->find(line) != std::string::npos;
    REQUIRE(in_full);
  }
}

TEST_CASE("symtab 规范化：确定性（两次调用一致）") {
  const auto raw = fs::read_file(fixture("mockelf.readelf-Ws.txt"));
  REQUIRE(raw);
  const auto a = w2ucem::verify::canonicalize_symtab(*raw, "full");
  const auto b = w2ucem::verify::canonicalize_symtab(*raw, "full");
  REQUIRE(a);
  REQUIRE(b);
  REQUIRE(*a == *b);
}

TEST_CASE("symtab 规范化：垃圾输入不崩溃且只含头部") {
  const auto snap = w2ucem::verify::canonicalize_symtab("这不是 readelf 输出\n乱码行\n", "full");
  REQUIRE(snap);
  REQUIRE(snap->rfind("w2ucem-symtab v1\n", 0) == 0);
  REQUIRE(snap->find("乱码行") == std::string::npos);
}

TEST_CASE("symtab 规范化：排序与去重（输入顺序无关）") {
  const auto raw = fs::read_file(fixture("mockelf.readelf-Ws.txt"));
  REQUIRE(raw);
  // 构造：raw + 重复的符号行（手工加一条重复行）
  const std::string doubled = *raw + "\n     6: 0000000000000000     0 FUNC    GLOBAL DEFAULT  UND puts@GLIBC_2.2.5\n";
  const auto a = w2ucem::verify::canonicalize_symtab(*raw, "full");
  const auto b = w2ucem::verify::canonicalize_symtab(doubled, "full");
  REQUIRE(a);
  REQUIRE(b);
  REQUIRE(*a == *b);  // 重复行被去重，结果不变
}

TEST_CASE("symtab 规范化：非法 scope 拒绝") {
  const auto r = w2ucem::verify::canonicalize_symtab("", "bogus");
  REQUIRE(!r);
  REQUIRE(r.error().code == ErrorCode::SchemaViolation);
}

TEST_CASE("validator：symtab 取值校验") {
  for (const auto& value : {"", "dyn", "full"}) {
    const auto tpl = TemplateParser::parse(template_with_symtab(value), "t");
    REQUIRE(tpl);
    REQUIRE(TemplateValidator::validate(*tpl));
  }
  const auto bad = TemplateParser::parse(template_with_symtab("bogus"), "t");
  REQUIRE(bad);
  const auto v = TemplateValidator::validate(*bad);
  REQUIRE(!v);
  REQUIRE(v.error().code == ErrorCode::SchemaViolation);
}

TEST_CASE("ledger：新字段往返与旧 JSON 兼容") {
  LedgerEntry entry;
  entry.build_id = "abc";
  entry.template_id = "tpl";
  entry.template_hash = "hash";
  entry.symtabs["files/bin/mockelf"] = std::string(64, 'c');
  entry.symtab_tool = "readelf";
  entry.buildenv_ref = "w2ucem/buildenv-alpine@sha256:" + std::string(64, '0');
  entry.dep_scan_skipped = {"arch"};

  const auto round = LedgerEntry::from_json(entry.to_json(), "test");
  REQUIRE(round);
  REQUIRE(round->symtabs.at("files/bin/mockelf") == std::string(64, 'c'));
  REQUIRE(round->symtab_tool == "readelf");
  REQUIRE(round->buildenv_ref == entry.buildenv_ref);
  REQUIRE(round->dep_scan_skipped == std::vector<std::string>{"arch"});

  // 旧 JSON（alpha 账本无新字段）→ 成功解析且新字段取默认
  const std::string old_json = R"({
    "build_id": "old", "template_id": "t", "template_hash": "h",
    "artifacts": {}, "dep_manifests": []
  })";
  const auto old = LedgerEntry::from_json(old_json, "test");
  REQUIRE(old);
  REQUIRE(old->symtabs.empty());
  REQUIRE(old->symtab_tool.empty());
  REQUIRE(old->buildenv_ref.empty());
}

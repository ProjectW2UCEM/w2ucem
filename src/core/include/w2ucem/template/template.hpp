// Template：解析后的模板（纯数据）。
// 函数式语义：模板 = 数据，函数体 = TemplateEvaluator::evaluate（随库版本锁定）。
// raw 保留原始 TOML——变体求值时在 TOML 层深合并后重新解析，
// 使 [variant.<gpu>] 能覆盖任意字段（未提及键继承 base）。
#pragma once

#include <map>
#include <string>
#include <vector>

#include <toml.hpp>

#include "w2ucem/template/assemble.hpp"
#include "w2ucem/template/build_depends.hpp"
#include "w2ucem/template/hooks.hpp"
#include "w2ucem/template/manifest.hpp"
#include "w2ucem/template/source.hpp"

namespace w2ucem {

struct TemplateMeta {
  std::string maintainer;
  std::string license;
  std::vector<std::string> channels;        // 通道取值域
  std::vector<std::string> gpu_variants;    // GPU 变体取值域
  std::vector<std::string> distro_families;  // 发行版族取值域
};

struct BuildEnvSpec {
  std::string image;   // 不可变构建镜像名
  std::string digest;  // 必填："sha256:<hex>"（registry digest 格式）
};

struct Template {
  std::string format;  // 模板格式版本，解析器硬校验
  std::string id;
  std::string name;
  std::string version;  // 模板自身版本
  std::string kind;     // alpha 仅 "assembly"
  std::string upstream;

  TemplateMeta meta;
  BuildEnvSpec buildenv;
  std::map<std::string, SourceSpec> sources;
  std::vector<PatchSpec> patches;
  BuildDepends build_depends;
  std::vector<AssembleStep> steps;
  std::map<std::string, toml::value> variants;  // key = gpu 变体名
  Hooks hooks;
  ManifestSpec manifest;

  toml::value raw;  // 原始 TOML（变体合并的底稿）
};

}  // namespace w2ucem

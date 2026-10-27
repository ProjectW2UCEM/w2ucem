// 构建依赖：包管理器式 build-depends 思想。
// family → 包名映射是"按发行版特调"的声明性载体（arch: libx11 / deb: libx11-dev）。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "w2ucem/channel/distro.hpp"

namespace w2ucem {

struct BuildDepends {
  std::vector<std::string> default_pkgs;
  std::map<DistroFamily, std::vector<std::string>> per_family;

  // 查询规则：family 有显式条目取之，否则回退 default。
  [[nodiscard]] const std::vector<std::string>& for_family(DistroFamily family) const;
};

}  // namespace w2ucem

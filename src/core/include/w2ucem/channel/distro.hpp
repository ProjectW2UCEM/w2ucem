// 发行版族与发行版标识。
// alpha：release（如 "stable"/"bookworm"/"39"）只记录并进入构建目录名，
// 不参与求值决策——决策维度收敛到 family。
#pragma once

#include <optional>
#include <string>

namespace w2ucem {

enum class DistroFamily {
  Debian,  // deb
  Rpm,     // rpm（Fedora/openSUSE）
  Arch,    // arch
  Alpine,  // alpine
};

[[nodiscard]] const char* family_str(DistroFamily family);
[[nodiscard]] std::optional<DistroFamily> family_from_str(const std::string& s);

struct Distro {
  DistroFamily family{DistroFamily::Debian};
  std::string release{"stable"};  // alpha：记录与目录命名用，不参与求值
};

}  // namespace w2ucem

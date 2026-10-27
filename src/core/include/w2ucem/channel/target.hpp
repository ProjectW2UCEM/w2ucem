// BuildTarget：求值器（纯函数）的参数元组。
// 同一 (template, BuildTarget, evaluator_version) 必然产出同一 BuildPlan。
#pragma once

#include <string>

#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/distro.hpp"
#include "w2ucem/channel/variant.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct BuildTarget {
  Channel channel{Channel::SemiRolling};
  GpuVariant gpu{GpuVariant::Generic};
  Distro distro{DistroFamily::Debian, "stable"};
  std::string arch{"x86_64"};  // alpha 仅记录，不参与求值
};

struct TargetParser {
  // 规范形式："semi-rolling:generic:deb:stable"（channel:gpu:family:release）
  // 亦接受 "stable:nvidia:arch"（release 缺省按 family 默认）。
  [[nodiscard]] static Result<BuildTarget> parse(const std::string& spec);

  [[nodiscard]] static std::string to_string(const BuildTarget& target);
};

}  // namespace w2ucem

#include "w2ucem/channel/target.hpp"

#include <sstream>
#include <vector>

#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/distro.hpp"
#include "w2ucem/channel/variant.hpp"
#include "w2ucem/util/error.hpp"

namespace w2ucem {

const char* channel_str(Channel channel) {
  switch (channel) {
    case Channel::SemiRolling: return "semi-rolling";
    case Channel::Stable: return "stable";
    case Channel::Major: return "major";
  }
  return "?";
}

std::optional<Channel> channel_from_str(const std::string& s) {
  if (s == "semi-rolling") return Channel::SemiRolling;
  if (s == "stable") return Channel::Stable;
  if (s == "major") return Channel::Major;
  return std::nullopt;
}

const char* gpu_variant_str(GpuVariant variant) {
  switch (variant) {
    case GpuVariant::Generic: return "generic";
    case GpuVariant::Nvidia: return "nvidia";
    case GpuVariant::Amd: return "amd";
    case GpuVariant::Intel: return "intel";
  }
  return "?";
}

std::optional<GpuVariant> gpu_variant_from_str(const std::string& s) {
  if (s == "generic") return GpuVariant::Generic;
  if (s == "nvidia") return GpuVariant::Nvidia;
  if (s == "amd") return GpuVariant::Amd;
  if (s == "intel") return GpuVariant::Intel;
  return std::nullopt;
}

const char* family_str(DistroFamily family) {
  switch (family) {
    case DistroFamily::Debian: return "deb";
    case DistroFamily::Rpm: return "rpm";
    case DistroFamily::Arch: return "arch";
    case DistroFamily::Alpine: return "alpine";
  }
  return "?";
}

std::optional<DistroFamily> family_from_str(const std::string& s) {
  if (s == "deb") return DistroFamily::Debian;
  if (s == "rpm") return DistroFamily::Rpm;
  if (s == "arch") return DistroFamily::Arch;
  if (s == "alpine") return DistroFamily::Alpine;
  return std::nullopt;
}

namespace {

// 各发行版族的 release 缺省值。
const char* default_release(DistroFamily family) {
  switch (family) {
    case DistroFamily::Debian: return "stable";
    case DistroFamily::Rpm: return "fedora";
    case DistroFamily::Arch: return "rolling";
    case DistroFamily::Alpine: return "edge";
  }
  return "";
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> parts;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, sep)) {
    parts.push_back(item);
  }
  return parts;
}

}  // namespace

Result<BuildTarget> TargetParser::parse(const std::string& spec) {
  const auto parts = split(spec, ':');
  if (parts.empty() || parts.size() > 4) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "target 须为 channel:gpu:family[:release] 形式", spec));
  }

  BuildTarget target;

  const auto channel = channel_from_str(parts[0]);
  if (!channel) {
    return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                      "未知通道: " + parts[0], spec));
  }
  target.channel = *channel;

  if (parts.size() >= 2) {
    const auto gpu = gpu_variant_from_str(parts[1]);
    if (!gpu) {
      return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                        "未知 GPU 变体: " + parts[1], spec));
    }
    target.gpu = *gpu;
  }

  if (parts.size() >= 3) {
    const auto family = family_from_str(parts[2]);
    if (!family) {
      return tl::unexpected(Error::make(ErrorCode::EvaluationError,
                                        "未知发行版族: " + parts[2], spec));
    }
    target.distro.family = *family;
    target.distro.release = parts.size() >= 4 ? parts[3] : default_release(*family);
  }

  return target;
}

std::string TargetParser::to_string(const BuildTarget& target) {
  return std::string(channel_str(target.channel)) + ":" + gpu_variant_str(target.gpu) + ":" +
         family_str(target.distro.family) + ":" + target.distro.release;
}

}  // namespace w2ucem

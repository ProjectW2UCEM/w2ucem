// GPU 变体：按显卡特调构建的维度之一。
#pragma once

#include <optional>
#include <string>

namespace w2ucem {

enum class GpuVariant {
  Generic,  // 通用
  Nvidia,
  Amd,
  Intel,
};

[[nodiscard]] const char* gpu_variant_str(GpuVariant variant);
[[nodiscard]] std::optional<GpuVariant> gpu_variant_from_str(const std::string& s);

}  // namespace w2ucem

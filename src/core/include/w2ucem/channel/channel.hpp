// 三通道（Debian 哲学）：半滚动 / 稳定 / 大版本。
#pragma once

#include <optional>
#include <string>

namespace w2ucem {

enum class Channel {
  SemiRolling,  // 半滚动：追踪上游最新，模板可含 record 模式 manifest
  Stable,       // 稳定：完全 pin，禁 record 模式
  Major,        // 大版本：冻结在特定大版本，规则同 Stable
};

[[nodiscard]] const char* channel_str(Channel channel);
[[nodiscard]] std::optional<Channel> channel_from_str(const std::string& s);

}  // namespace w2ucem

// CLI 输出工具：--json 机器可读 / 人类可读两种模式。
// 未来 QT/GTK/SwiftUI 前端复用同一 Facade，本工具仅服务于 CLI。
#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <string>

#include <nlohmann/json.hpp>

#include "w2ucem/util/error.hpp"

namespace w2ucem::cli {

class Output {
 public:
  explicit Output(bool json) : json_(json) {}

  [[nodiscard]] bool is_json() const { return json_; }

  void text(const std::string& line) const {
    if (!json_) {
      fmt::print("{}\n", line);
    }
  }

  void json(const nlohmann::json& j) const {
    if (json_) {
      fmt::print("{}\n", j.dump(2));
    }
  }

  void error(const std::string& message) const {
    if (json_) {
      fmt::print("{}\n", nlohmann::json{{"error", message}}.dump());
    } else {
      fmt::print("[error] {}\n", message);
    }
  }

 private:
  bool json_;
};

// 统一错误输出格式：错误码 + 消息 + 上下文。
inline std::string format_error(const w2ucem::Error& e) {
  return std::string(w2ucem::error_code_str(e.code)) + ": " + e.message +
         (e.context.empty() ? "" : " [" + e.context + "]");
}

}  // namespace w2ucem::cli

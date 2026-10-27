// LogSink 抽象 + 全局 Logger：前端无关机制的第三根支柱。
// CLI 实现为 stderr 输出；TUI 实现为互斥锁保护的环形缓冲事件队列；
// 构建日志另有文件 sink。核心库永不直接写 stdout。
#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <fmt/format.h>

namespace w2ucem {

enum class LogLevel { Debug, Info, Warn, Error };

class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void write(LogLevel level, const std::string& message) = 0;
};

class Logger {
 public:
  static Logger& instance();

  // 线程安全：sink 指针以 mutex 保护，构建线程与 UI 线程可并发写入。
  void set_sink(std::shared_ptr<LogSink> sink);
  void log(LogLevel level, const std::string& message);

  template <typename... Args>
  void debug(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::Debug, fmt::format(fmt_str, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void info(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::Info, fmt::format(fmt_str, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void warn(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::Warn, fmt::format(fmt_str, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void error(fmt::format_string<Args...> fmt_str, Args&&... args) {
    log(LogLevel::Error, fmt::format(fmt_str, std::forward<Args>(args)...));
  }

 private:
  Logger() = default;
  std::shared_ptr<LogSink> sink_;
  std::mutex mutex_;
};

// 默认 sink：写 stderr（CLI 用），带级别前缀。
class StderrLogSink : public LogSink {
 public:
  void write(LogLevel level, const std::string& message) override;
};

}  // namespace w2ucem

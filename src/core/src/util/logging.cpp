#include "w2ucem/util/logging.hpp"

#include <iostream>

namespace w2ucem {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::set_sink(std::shared_ptr<LogSink> sink) {
  std::lock_guard lock(mutex_);
  sink_ = std::move(sink);
}

void Logger::log(LogLevel level, const std::string& message) {
  std::lock_guard lock(mutex_);
  if (sink_) {
    sink_->write(level, message);
  }
}

void StderrLogSink::write(LogLevel level, const std::string& message) {
  const char* prefix = "";
  switch (level) {
    case LogLevel::Debug: prefix = "[debug] "; break;
    case LogLevel::Info: prefix = "[info] "; break;
    case LogLevel::Warn: prefix = "[warn] "; break;
    case LogLevel::Error: prefix = "[error] "; break;
  }
  std::cerr << prefix << message << '\n';
}

}  // namespace w2ucem

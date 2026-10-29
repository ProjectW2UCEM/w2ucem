#include <cstdint>

#include "w2ucem/util/fs.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#if !defined(_WIN32)
#include <unistd.h>  // getpid
#endif

namespace w2ucem::fs {

Result<void> atomic_write(const std::string& path, const std::string& bytes) {
  const std::string tmp = path + ".tmp." + std::to_string(
#if defined(_WIN32)
      0
#else
      getpid()
#endif
  );
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return tl::unexpected(Error::make(ErrorCode::Io, "无法创建临时文件", tmp));
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) {
      return tl::unexpected(Error::make(ErrorCode::Io, "写入临时文件失败", tmp));
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    return tl::unexpected(Error::make(ErrorCode::Io, "rename 失败: " + ec.message(), path));
  }
  return {};
}

Result<void> ensure_dir(const std::string& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    return tl::unexpected(Error::make(ErrorCode::Io, "创建目录失败: " + ec.message(), path));
  }
  return {};
}

Result<std::string> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return tl::unexpected(Error::make(ErrorCode::Io, "无法打开文件", path));
  }
  std::ostringstream oss;
  oss << in.rdbuf();
  if (in.bad()) {
    return tl::unexpected(Error::make(ErrorCode::Io, "读取文件失败", path));
  }
  return oss.str();
}

Result<std::uintmax_t> file_size(const std::string& path) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec == std::errc::no_such_file_or_directory) {
    return 0;
  }
  if (ec) {
    return tl::unexpected(Error::make(ErrorCode::Io, "stat 失败: " + ec.message(), path));
  }
  return size;
}

Result<void> remove_file(const std::string& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
  if (ec && ec != std::errc::no_such_file_or_directory) {
    return tl::unexpected(Error::make(ErrorCode::Io, "删除失败: " + ec.message(), path));
  }
  return {};
}

bool exists(const std::string& path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

bool is_dir(const std::string& path) {
  std::error_code ec;
  return std::filesystem::is_directory(path, ec);
}

}  // namespace w2ucem::fs

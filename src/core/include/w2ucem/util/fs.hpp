// 文件系统工具：原子写、目录保证、读文件。
// atomic_write 是 ledger 与索引缓存落盘一致性的基础（临时文件 + rename）。
#pragma once

#include <string>

#include "w2ucem/util/result.hpp"

namespace w2ucem::fs {

// 原子写：写 <path>.tmp.<pid> 后 rename 到 path，保证观察者不会读到半截文件。
[[nodiscard]] Result<void> atomic_write(const std::string& path, const std::string& bytes);

[[nodiscard]] Result<void> ensure_dir(const std::string& path);

[[nodiscard]] Result<std::string> read_file(const std::string& path);

// 文件大小；不存在返回 0（续传状态机把缺失视为空 partial）。
[[nodiscard]] Result<std::uintmax_t> file_size(const std::string& path);

// 删除文件；不存在视为成功。
[[nodiscard]] Result<void> remove_file(const std::string& path);

[[nodiscard]] bool exists(const std::string& path);

[[nodiscard]] bool is_dir(const std::string& path);

}  // namespace w2ucem::fs

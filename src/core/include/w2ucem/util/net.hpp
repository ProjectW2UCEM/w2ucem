// net：宿主侧网络下载（libcurl）。设计决策：网络只在宿主，容器零网络。
// 支持 https:// 与 file://（及纯本地路径，测试/仓库扮演用）。
#pragma once

#include <string>

#include "w2ucem/util/result.hpp"

namespace w2ucem::net {

// 下载到内存（小文件：索引、模板）。
[[nodiscard]] Result<std::string> get(const std::string& url);

// 下载到文件（大文件：源 tarball）。文件已存在则覆盖。
[[nodiscard]] Result<void> get_to_file(const std::string& url, const std::string& dst_path);

}  // namespace w2ucem::net

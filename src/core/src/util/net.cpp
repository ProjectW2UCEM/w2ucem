#include "w2ucem/util/net.hpp"

#include <curl/curl.h>

#include <fstream>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"

namespace w2ucem::net {

namespace {

bool is_https(const std::string& url) {
  return url.rfind("https://", 0) == 0;
}

std::string file_url_to_path(const std::string& url) {
  // file:///abs/path → /abs/path；file://rel → rel
  return url.substr(7);
}

std::size_t write_to_string(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  const auto total = size * nmemb;
  static_cast<std::string*>(userdata)->append(ptr, total);
  return total;
}

std::size_t write_to_file(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  const auto total = size * nmemb;
  static_cast<std::ofstream*>(userdata)->write(ptr, static_cast<std::streamsize>(total));
  return total;
}

Result<std::string> curl_get(const std::string& url, const std::string& dst_path /* 空 = 内存 */) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return tl::unexpected(Error::make(ErrorCode::Network, "libcurl 初始化失败", url));
  }

  std::string body;
  std::ofstream out;
  if (!dst_path.empty()) {
    out.open(dst_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      curl_easy_cleanup(curl);
      return tl::unexpected(Error::make(ErrorCode::Io, "无法创建下载目标文件", dst_path));
    }
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "w2ucem/0.1-alpha");
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  if (dst_path.empty()) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  } else {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
  }

  const CURLcode rc = curl_easy_perform(curl);
  const auto error =
      rc == CURLE_OK ? "" : std::string(curl_easy_strerror(rc));
  curl_easy_cleanup(curl);
  out.flush();
  if (out.bad()) {
    return tl::unexpected(Error::make(ErrorCode::Io, "写入下载文件失败", dst_path));
  }
  if (rc != CURLE_OK) {
    return tl::unexpected(Error::make(ErrorCode::Network, "下载失败: " + error, url));
  }
  return body;
}

}  // namespace

Result<std::string> get(const std::string& url) {
  if (is_https(url)) {
    return curl_get(url, "");
  }
  // file:// 或纯本地路径
  const std::string path = url.rfind("file://", 0) == 0 ? file_url_to_path(url) : url;
  return fs::read_file(path);
}

Result<void> get_to_file(const std::string& url, const std::string& dst_path) {
  if (is_https(url)) {
    auto r = curl_get(url, dst_path);
    if (!r) {
      return tl::unexpected(std::move(r.error()));
    }
    return {};
  }
  const std::string path = url.rfind("file://", 0) == 0 ? file_url_to_path(url) : url;
  auto data = fs::read_file(path);
  if (!data) {
    return tl::unexpected(std::move(data.error()));
  }
  return fs::atomic_write(dst_path, *data);
}

}  // namespace w2ucem::net

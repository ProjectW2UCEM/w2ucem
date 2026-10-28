#include "w2ucem/util/net.hpp"

#include <curl/curl.h>

#include <fstream>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"

namespace w2ucem::net {

namespace {

bool is_http(const std::string& url) {
  return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
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

std::size_t header_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  const auto total = size * nmemb;
  static_cast<std::string*>(userdata)->append(ptr, total);
  return total;
}

// 从响应头首行解析 HTTP 状态码；无 HTTP 响应（连接层失败）返回 0。
long parse_http_code(const std::string& headers) {
  if (headers.rfind("HTTP/", 0) != 0) {
    return 0;
  }
  const auto space = headers.find(' ');
  if (space == std::string::npos) {
    return 0;
  }
  std::string code;
  for (std::size_t i = space + 1; i < headers.size() && headers[i] >= '0' && headers[i] <= '9';
       ++i) {
    code += headers[i];
  }
  return code.empty() ? 0 : std::stol(code);
}

struct CurlTransferResult {
  CURLcode rc{CURLE_OK};
  long http_code{0};  // 0 = 无 HTTP 响应（连接层失败）
};

// 单次 curl 下载：resume_from > 0 时发 Range 续传（append 打开），否则全量（trunc 打开）。
// 注意：FAILONERROR 的 4xx/5xx 在 header 回调执行后才作为错误返回，
// 因此 http_code（含 416）一定先被捕获。
CurlTransferResult curl_download(const std::string& url, const std::string& dst_path,
                                 curl_off_t resume_from, bool append_mode) {
  CurlTransferResult result;

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    result.rc = CURLE_FAILED_INIT;
    return result;
  }

  std::ofstream out(dst_path, std::ios::binary |
                                  (append_mode ? std::ios::app : std::ios::trunc));
  if (!out) {
    curl_easy_cleanup(curl);
    result.rc = CURLE_WRITE_ERROR;
    return result;
  }

  std::string headers;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "w2ucem/0.1-alpha");
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
  if (resume_from > 0) {
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resume_from);
  }

  result.rc = curl_easy_perform(curl);
  result.http_code = parse_http_code(headers);
  curl_easy_cleanup(curl);
  out.flush();
  if (out.bad()) {
    result.rc = CURLE_WRITE_ERROR;
  }
  return result;
}

// 全量下载（trunc 重下）；失败处理：4xx 删除 dst，网络中断保留 partial。
Result<void> fresh_download(const std::string& url, const std::string& dst_path) {
  const auto r = curl_download(url, dst_path, 0, false);
  if (r.rc == CURLE_OK) {
    return {};
  }
  if (r.http_code >= 400) {
    const auto removed = fs::remove_file(dst_path);
    (void)removed;
    return tl::unexpected(Error::make(
        ErrorCode::Network,
        "下载失败: HTTP " + std::to_string(r.http_code) + " (" + curl_easy_strerror(r.rc) + ")",
        url));
  }
  return tl::unexpected(Error::make(ErrorCode::Network,
                                    "下载中断: " + std::string(curl_easy_strerror(r.rc)) +
                                        "（partial 已保留，可续传）",
                                    url));
}

Result<void> https_download(const std::string& url, const std::string& dst_path) {
  // ---------- 无 partial：全新下载 ----------
  const auto size_r = fs::file_size(dst_path);
  if (!size_r) {
    return tl::unexpected(std::move(size_r.error()));
  }
  const auto partial = static_cast<curl_off_t>(*size_r);
  if (partial == 0) {
    return fresh_download(url, dst_path);
  }

  // ---------- 有 partial：Range 续传 ----------
  const auto r = curl_download(url, dst_path, partial, true);

  // 续传完成
  if (r.rc == CURLE_OK && r.http_code == 206) {
    return {};
  }
  // libcurl 对 RESUME 收到非 206 响应时报 CURLE_RANGE_ERROR 且保证
  // 未写入任何数据（partial 未被污染）——回退全量重下。
  if (r.rc == CURLE_RANGE_ERROR) {
    return fresh_download(url, dst_path);
  }
  // Range 越界（partial ≥ 服务器文件）：回退全量
  if (r.http_code == 416) {
    return fresh_download(url, dst_path);
  }
  // 其他 4xx/5xx：删除错误页
  if (r.http_code >= 400) {
    const auto removed = fs::remove_file(dst_path);
    (void)removed;
    return tl::unexpected(Error::make(
        ErrorCode::Network,
        "下载失败: HTTP " + std::to_string(r.http_code) + " (" + curl_easy_strerror(r.rc) + ")",
        url));
  }
  // 网络中断：无 HTTP 响应（连接层失败，partial 未写入）或 206 截断
  // （续传数据正确追加），两种情况 partial 都保留可续。
  return tl::unexpected(Error::make(ErrorCode::Network,
                                    "下载中断: " + std::string(curl_easy_strerror(r.rc)) +
                                        "（partial 已保留，可续传）",
                                    url));
}

Result<std::string> curl_get(const std::string& url) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return tl::unexpected(Error::make(ErrorCode::Network, "libcurl 初始化失败", url));
  }

  std::string body;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "w2ucem/0.1-alpha");
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

  const CURLcode rc = curl_easy_perform(curl);
  const auto error = rc == CURLE_OK ? "" : std::string(curl_easy_strerror(rc));
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK) {
    return tl::unexpected(Error::make(ErrorCode::Network, "下载失败: " + error, url));
  }
  return body;
}

}  // namespace

Result<std::string> get(const std::string& url) {
  if (is_http(url)) {
    return curl_get(url);
  }
  // file:// 或纯本地路径
  const std::string path = url.rfind("file://", 0) == 0 ? file_url_to_path(url) : url;
  return fs::read_file(path);
}

Result<void> get_to_file(const std::string& url, const std::string& dst_path) {
  if (is_http(url)) {
    return https_download(url, dst_path);
  }
  const std::string path = url.rfind("file://", 0) == 0 ? file_url_to_path(url) : url;
  auto data = fs::read_file(path);
  if (!data) {
    return tl::unexpected(std::move(data.error()));
  }
  return fs::atomic_write(dst_path, *data);
}

}  // namespace w2ucem::net

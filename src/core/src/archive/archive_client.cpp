#include "w2ucem/archive/archive_client.hpp"

#include <filesystem>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/net.hpp"
#include "w2ucem/util/sha256.hpp"

namespace w2ucem {

Result<std::string> ArchiveClient::fetch_to_cache(const std::string& archive_id,
                                                  const std::vector<std::string>& archive_urls,
                                                  const std::string& blob_cache_dir) {
  if (!archive_id.starts_with("sha256:") || !Sha256::is_valid_hex(archive_id.substr(7))) {
    return tl::unexpected(
        Error::make(ErrorCode::SchemaViolation, "非法 archive_id（须为 sha256:<hex>）", archive_id));
  }
  const std::string hexpart = archive_id.substr(7);
  const std::string local = blob_cache_dir + "/" + hexpart;

  // 本地缓存命中：落缓存前已校验，直接返回
  if (fs::exists(local)) {
    return local;
  }

  Error last_err = Error::make(ErrorCode::MirrorExhausted, "全部时光机镜像不可达", archive_id);
  if (auto r = fs::ensure_dir(blob_cache_dir); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  for (const auto& url : archive_urls) {
    const std::string remote = url + "/blobs/" + hexpart;
    const std::string tmp = local + ".download";
    if (auto r = net::get_to_file(remote, tmp); !r) {
      last_err = std::move(r.error());
      continue;
    }
    const auto digest = Sha256::of_file(tmp);
    if (!digest) {
      last_err = std::move(digest.error());
      continue;
    }
    if (Sha256::hex(*digest) != hexpart) {
      last_err = Error::make(ErrorCode::HashMismatch,
                             "时光机 blob 内容哈希与 id 不符", archive_id);
      continue;
    }
    if (auto r = fs::ensure_dir(blob_cache_dir); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    std::error_code ec;
    std::filesystem::rename(tmp, local, ec);
    if (ec) {
      return tl::unexpected(Error::make(ErrorCode::Io, "blob 落缓存失败: " + ec.message(), local));
    }
    return local;
  }

  return tl::unexpected(std::move(last_err));
}

Result<std::string> ArchiveClient::add(const std::string& file_path,
                                       const std::string& blob_cache_dir) {
  const auto digest = Sha256::of_file(file_path);
  if (!digest) {
    return tl::unexpected(std::move(digest.error()));
  }
  const std::string hexpart = Sha256::hex(*digest);
  const std::string local = blob_cache_dir + "/" + hexpart;
  if (!fs::exists(local)) {
    if (auto r = fs::ensure_dir(blob_cache_dir); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    std::error_code ec;
    std::filesystem::copy_file(file_path, local, std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
      return tl::unexpected(Error::make(ErrorCode::Io, "ingest 拷贝失败: " + ec.message(), local));
    }
  }
  return "sha256:" + hexpart;
}

}  // namespace w2ucem

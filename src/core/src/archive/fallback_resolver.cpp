#include "w2ucem/archive/fallback_resolver.hpp"

#include <filesystem>

#include "w2ucem/archive/archive_client.hpp"
#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/net.hpp"
#include "w2ucem/util/sha256.hpp"

namespace w2ucem {

Result<std::string> FallbackSourceResolver::resolve(const ResolvedSource& source,
                                                    const std::vector<std::string>& archive_urls,
                                                    const std::string& cache_dir,
                                                    const std::string& blob_dir,
                                                    bool force_archive) {
  const std::string cached = cache_dir + "/" + source.sha256;

  // 内容寻址缓存命中：此前已校验
  if (fs::exists(cached)) {
    return cached;
  }

  Error last_err = Error::make(ErrorCode::MirrorExhausted, "无可用镜像", source.id);

  if (auto r = fs::ensure_dir(cache_dir); !r) {
    return tl::unexpected(std::move(r.error()));
  }

  // 官方 urls 逐个尝试
  if (!force_archive) {
    for (const auto& url : source.urls) {
      const std::string tmp = cached + ".download";
      if (auto r = net::get_to_file(url, tmp); !r) {
        last_err = std::move(r.error());
        continue;
      }
      const auto digest = Sha256::of_file(tmp);
      if (!digest) {
        last_err = std::move(digest.error());
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        continue;
      }
      if (Sha256::hex(*digest) != source.sha256) {
        last_err = Error::make(ErrorCode::HashMismatch, "源哈希与模板声明不符", url);
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        continue;
      }
      if (auto r = fs::ensure_dir(cache_dir); !r) {
        return tl::unexpected(std::move(r.error()));
      }
      std::error_code ec;
      std::filesystem::rename(tmp, cached, ec);
      if (ec) {
        return tl::unexpected(
            Error::make(ErrorCode::Io, "源落缓存失败: " + ec.message(), cached));
      }
      return cached;
    }
  }

  // 时光机回退：官方源全部失败（或强制 archive）→ ArchiveClient
  if (!source.archive_id.empty()) {
    auto blob = ArchiveClient::fetch_to_cache(source.archive_id, archive_urls, blob_dir);
    if (!blob) {
      last_err = std::move(blob.error());
    } else {
      const auto digest = Sha256::of_file(*blob);
      if (!digest || Sha256::hex(*digest) != source.sha256) {
        last_err = Error::make(ErrorCode::HashMismatch,
                               "时光机 blob 与源声明哈希不一致", source.id);
      } else {
        if (auto r = fs::ensure_dir(cache_dir); !r) {
          return tl::unexpected(std::move(r.error()));
        }
        std::error_code ec;
        std::filesystem::copy_file(*blob, cached,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
          return tl::unexpected(
              Error::make(ErrorCode::Io, "源回写缓存失败: " + ec.message(), cached));
        }
        return cached;
      }
    }
  }

  return tl::unexpected(std::move(last_err));
}

}  // namespace w2ucem

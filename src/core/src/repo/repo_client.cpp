#include "w2ucem/repo/repo_client.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"
#include "w2ucem/util/net.hpp"
#include "w2ucem/util/sha256.hpp"

namespace w2ucem {

namespace {

// ed25519 验签（sig 为 64 字节原始签名，pubkey 为 PEM）。
Result<void> verify_ed25519(const std::string& data, const std::string& sig,
                           const std::string& pubkey_path) {
  if (pubkey_path.empty() || !fs::exists(pubkey_path)) {
    return tl::unexpected(Error::make(ErrorCode::SignatureInvalid,
                                      "缺少仓库签名公钥（请配置 sig_pubkey_file 或放置默认公钥）",
                                      pubkey_path));
  }
  BIO* bio = BIO_new_file(pubkey_path.c_str(), "rb");
  if (bio == nullptr) {
    return tl::unexpected(
        Error::make(ErrorCode::SignatureInvalid, "公钥文件打开失败", pubkey_path));
  }
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (pkey == nullptr) {
    return tl::unexpected(
        Error::make(ErrorCode::SignatureInvalid, "公钥解析失败（须为 PEM 格式）", pubkey_path));
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  const int ok = ctx != nullptr &&
                 EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1 &&
                 EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char*>(sig.data()),
                                  sig.size(), reinterpret_cast<const unsigned char*>(data.data()),
                                  data.size()) == 1;
  if (ctx != nullptr) {
    EVP_MD_CTX_free(ctx);
  }
  EVP_PKEY_free(pkey);

  if (ok != 1) {
    return tl::unexpected(
        Error::make(ErrorCode::SignatureInvalid, "仓库索引签名校验失败", pubkey_path));
  }
  return {};
}

}  // namespace

Result<RepoIndex> RepoClient::sync(const std::vector<std::string>& repo_urls,
                                   const std::string& sig_pubkey_path, bool allow_unsigned,
                                   const std::string& local_index_dir) {
  Error last_err = Error::make(ErrorCode::MirrorExhausted, "全部仓库镜像不可达");

  for (const auto& url : repo_urls) {
    auto index_bytes = net::get(url + "/repo.db");
    if (!index_bytes) {
      last_err = std::move(index_bytes.error());
      continue;
    }

    // 验签（签名存在则必须通过；无签名仅 allow_unsigned 时放行——测试语义）
    auto sig_bytes = net::get(url + "/repo.db.sig");
    if (sig_bytes) {
      auto v = verify_ed25519(*index_bytes, *sig_bytes, sig_pubkey_path);
      if (!v) {
        last_err = std::move(v.error());
        continue;
      }
    } else if (!allow_unsigned) {
      last_err = Error::make(ErrorCode::SignatureInvalid,
                             "仓库未提供 repo.db.sig 且 allow_unsigned_index=false", url);
      continue;
    }

    // 先解析后落盘：坏索引永不进入本地缓存
    auto idx = RepoIndex::from_json(*index_bytes, url);
    if (!idx) {
      last_err = std::move(idx.error());
      continue;
    }

    if (auto r = fs::ensure_dir(local_index_dir); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    if (auto r = fs::atomic_write(local_index_dir + "/repo.db", *index_bytes); !r) {
      return tl::unexpected(std::move(r.error()));
    }
    if (sig_bytes) {
      if (auto r = fs::atomic_write(local_index_dir + "/repo.db.sig", *sig_bytes); !r) {
        return tl::unexpected(std::move(r.error()));
      }
    }
    return idx;
  }

  return tl::unexpected(std::move(last_err));
}

Result<std::string> RepoClient::fetch_template(const IndexedTemplate& entry,
                                               const std::vector<std::string>& repo_urls) {
  Error last_err = Error::make(ErrorCode::MirrorExhausted, "全部仓库镜像不可达", entry.id);

  for (const auto& url : repo_urls) {
    auto bytes = net::get(url + "/" + entry.path);
    if (!bytes) {
      last_err = std::move(bytes.error());
      continue;
    }
    if (Sha256::hex(Sha256::of_bytes(*bytes)) != entry.sha256) {
      last_err = Error::make(ErrorCode::HashMismatch,
                             "模板字节哈希与索引不符（模板被篡改或索引过期）",
                             entry.id + "@" + entry.version);
      continue;
    }
    return bytes;
  }

  return tl::unexpected(std::move(last_err));
}

Result<RepoIndex> RepoClient::read_local_index(const std::string& local_index_dir) {
  const std::string path = local_index_dir + "/repo.db";
  auto bytes = fs::read_file(path);
  if (!bytes) {
    return tl::unexpected(Error::make(ErrorCode::Io, "本地仓库索引不存在，请先 `w2ucem sync`", path));
  }
  return RepoIndex::from_json(*bytes, path);
}

}  // namespace w2ucem

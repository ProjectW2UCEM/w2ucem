#include "w2ucem/util/sha256.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>

namespace w2ucem {

namespace {

[[nodiscard]] bool hex_nibble(char c, std::uint8_t& out) {
  if (c >= '0' && c <= '9') {
    out = static_cast<std::uint8_t>(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    out = static_cast<std::uint8_t>(c - 'a' + 10);
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    out = static_cast<std::uint8_t>(c - 'A' + 10);
    return true;
  }
  return false;
}

}  // namespace

Result<Sha256Digest> Sha256::of_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return tl::unexpected(Error::make(ErrorCode::Io, "无法打开文件", path));
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    return tl::unexpected(Error::make(ErrorCode::Io, "EVP 上下文创建失败", path));
  }

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    return tl::unexpected(Error::make(ErrorCode::Io, "EVP 初始化失败", path));
  }

  std::array<char, 65536> buf{};
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const auto n = in.gcount();
    if (n > 0 && EVP_DigestUpdate(ctx, buf.data(), static_cast<std::size_t>(n)) != 1) {
      EVP_MD_CTX_free(ctx);
      return tl::unexpected(Error::make(ErrorCode::Io, "EVP 更新失败", path));
    }
  }
  if (in.bad()) {
    EVP_MD_CTX_free(ctx);
    return tl::unexpected(Error::make(ErrorCode::Io, "读取文件失败", path));
  }

  Sha256Digest digest{};
  unsigned int out_len = 0;
  if (EVP_DigestFinal_ex(ctx, digest.data(), &out_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return tl::unexpected(Error::make(ErrorCode::Io, "EVP 终结失败", path));
  }
  EVP_MD_CTX_free(ctx);
  return digest;
}

Sha256Digest Sha256::of_bytes(const void* data, std::size_t len) {
  Sha256Digest digest{};
  unsigned int out_len = 0;
  EVP_Digest(data, len, digest.data(), &out_len, EVP_sha256(), nullptr);
  return digest;
}

Sha256Digest Sha256::of_bytes(const std::string& data) {
  return of_bytes(data.data(), data.size());
}

std::string Sha256::hex(const Sha256Digest& digest) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    oss << std::setw(2) << static_cast<unsigned>(byte);
  }
  return oss.str();
}

bool Sha256::is_valid_hex(const std::string& hex) {
  if (hex.size() != 64) {
    return false;
  }
  std::uint8_t unused = 0;
  for (const char c : hex) {
    if (!hex_nibble(c, unused)) {
      return false;
    }
  }
  return true;
}

Result<Sha256Digest> Sha256::from_hex(const std::string& hex) {
  if (!is_valid_hex(hex)) {
    return tl::unexpected(Error::make(ErrorCode::SchemaViolation,
                                      "非法的 sha256 hex（须为 64 位十六进制）", hex));
  }
  Sha256Digest digest{};
  for (std::size_t i = 0; i < 32; ++i) {
    std::uint8_t hi = 0;
    std::uint8_t lo = 0;
    hex_nibble(hex[i * 2], hi);
    hex_nibble(hex[i * 2 + 1], lo);
    digest[i] = static_cast<std::uint8_t>((hi << 4U) | lo);
  }
  return digest;
}

Result<Sha256Digest> Sha256::parse_or_error(const std::string& hex, const std::string& context) {
  auto r = from_hex(hex);
  if (!r) {
    r.error().context = context;
  }
  return r;
}

}  // namespace w2ucem

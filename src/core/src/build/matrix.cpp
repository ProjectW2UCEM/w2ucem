#include "w2ucem/build/matrix.hpp"

#include <cstdlib>

#include <toml.hpp>

#include "w2ucem/util/error.hpp"
#include "w2ucem/util/fs.hpp"

#ifndef W2UCEM_MATRIX_FILE
#define W2UCEM_MATRIX_FILE "ci/matrix.toml"
#endif

namespace w2ucem {

Result<std::map<DistroFamily, MatrixCell>> DistroMatrix::load_from_file(
    const std::string& path) {
  std::map<DistroFamily, MatrixCell> cells;

  toml::value root;
  try {
    root = toml::parse(path);
  } catch (const std::exception& e) {
    return tl::unexpected(Error::make(ErrorCode::Parse, e.what(), path));
  }

  try {
    for (const auto& [key, v] : root.as_table()) {
      const auto family = family_from_str(key);
      if (!family) {
        continue;  // 未知族条目忽略（矩阵表由官方维护）
      }
      MatrixCell cell;
      cell.image = toml::find<std::string>(v, "image");
      cell.digest = toml::find<std::string>(v, "digest");
      cells[*family] = std::move(cell);
    }
  } catch (const std::exception& e) {
    return tl::unexpected(Error::make(ErrorCode::Parse, e.what(), path));
  }
  return cells;
}

Result<const DistroMatrix*> DistroMatrix::load() {
  static DistroMatrix matrix;
  static bool loaded = false;
  if (loaded) {
    return &matrix;
  }

  const char* env = std::getenv("W2UCEM_MATRIX_FILE");
  const std::string path = (env != nullptr && *env != '\0') ? env : W2UCEM_MATRIX_FILE;

  auto cells = load_from_file(path);
  if (!cells) {
    return tl::unexpected(std::move(cells.error()));
  }
  matrix.cells_ = std::move(*cells);
  loaded = true;
  return &matrix;
}

std::optional<MatrixCell> DistroMatrix::cell(DistroFamily family) const {
  const auto it = cells_.find(family);
  if (it == cells_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace w2ucem

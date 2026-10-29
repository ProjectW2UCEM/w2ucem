// DistroMatrix：编译进二进制的发行版构建矩阵（镜像 + digest 锁定）。
// 环境变量 W2UCEM_MATRIX_FILE 可覆盖默认矩阵表（CI e2e 动态注入本地构建镜像 digest）。
#pragma once

#include <map>
#include <optional>
#include <string>

#include "w2ucem/channel/distro.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

struct MatrixCell {
  std::string image;
  std::string digest;
};

class DistroMatrix {
 public:
  // 返回单例指针（tl::expected 不支持引用类型，故用指针）。
  [[nodiscard]] static Result<const DistroMatrix*> load();

  // 从指定文件解析（load() 内部使用；抽出以便单测绕过进程级单例/环境变量）。
  [[nodiscard]] static Result<std::map<DistroFamily, MatrixCell>> load_from_file(
      const std::string& path);

  [[nodiscard]] std::optional<MatrixCell> cell(DistroFamily family) const;
  [[nodiscard]] const std::map<DistroFamily, MatrixCell>& cells() const { return cells_; }

 private:
  std::map<DistroFamily, MatrixCell> cells_;
};

}  // namespace w2ucem

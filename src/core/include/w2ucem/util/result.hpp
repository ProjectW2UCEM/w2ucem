// Result<T>：tl::expected 别名 + Error。
// 用法：Result<Foo> f();  f.has_value() / f.error().code == ErrorCode::HashMismatch
#pragma once

#include <tl/expected.hpp>

#include "w2ucem/util/error.hpp"

namespace w2ucem {

template <typename T>
using Result = tl::expected<T, Error>;

}  // namespace w2ucem

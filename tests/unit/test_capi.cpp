#include <catch2/catch_test_macros.hpp>

#include "w2ucem/capi/w2ucem_capi.h"

// alpha：C ABI 仅验证接口存在性（1.0 前不承诺 ABI 稳定）。
TEST_CASE("C ABI 版本接口可用") {
  REQUIRE(w2ucem_capi_version() == W2UCEM_CAPI_VERSION);
}

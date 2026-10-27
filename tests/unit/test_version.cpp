#include <catch2/catch_test_macros.hpp>

#include "w2ucem/version.hpp"

TEST_CASE("版本常量一致且非空") {
  REQUIRE(std::string(w2ucem::W2UCEM_VERSION).find("0.1.0") != std::string::npos);
  REQUIRE(w2ucem::EVALUATOR_VERSION[0] != '\0');
  REQUIRE(w2ucem::TEMPLATE_FORMAT_VERSION[0] != '\0');
  REQUIRE(w2ucem::W2UCEM_CAPI_VERSION >= 0);
}

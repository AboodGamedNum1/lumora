#include <catch2/catch_test_macros.hpp>

#include "lumora/version.h"

TEST_CASE("version string names the product") {
  const std::string v = lumora::version_string();
  REQUIRE(v.rfind("Lumora ", 0) == 0);
  REQUIRE(v.size() > 7);
}

TEST_CASE("toolchain string is not empty") {
  REQUIRE_FALSE(lumora::toolchain_string().empty());
}

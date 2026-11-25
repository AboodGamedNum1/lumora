#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "lumora/version.h"

TEST_CASE("version string names the product") {
  const std::string v = lumora::version_string();
  REQUIRE(v.rfind("Lumora ", 0) == 0);
  REQUIRE(v.size() > 7);
}

TEST_CASE("toolchain string is not empty") {
  REQUIRE_FALSE(lumora::toolchain_string().empty());
}

TEST_CASE("golden comparison respects the tolerance") {
  golden::Image a{2, 1, {10, 20, 30, 40, 50, 60}};
  golden::Image b = a;
  CHECK(golden::compare(a, b, 0).ok);
  b.rgb[4] = 52;
  CHECK(golden::compare(a, b, 2).ok);
  golden::Result r = golden::compare(a, b, 1);
  CHECK_FALSE(r.ok);
  CHECK(r.bad_pixels == 1);
  CHECK(r.max_delta == 2);
  golden::Image c{1, 1, {0, 0, 0}};
  CHECK_FALSE(golden::compare(a, c, 255).ok);  // size mismatch
}

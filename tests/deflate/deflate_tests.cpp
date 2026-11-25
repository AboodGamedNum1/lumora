// Placeholder owned by the deflate contributor. Replace with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "deflate.h"

TEST_CASE("deflate stub builds", "[deflate]") {
  const auto r = lumora::package::inflate_raw(nullptr, 0, 16);
  CHECK(r.status != lumora::package::InflateStatus::Ok);
}

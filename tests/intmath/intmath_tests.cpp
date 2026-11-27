// Tests for core/intmath. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "intmath.h"

TEST_CASE("intmath stub is present", "[intmath]") {
  CHECK(lumora::intmath::gcd_u64(0, 0) == 0);
}

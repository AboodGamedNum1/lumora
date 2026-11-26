// Tests for core/fixed. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "fixed.h"

TEST_CASE("fixed stub is present", "[fixed]") {
  CHECK(lumora::fixed::kOne == 65536);
}

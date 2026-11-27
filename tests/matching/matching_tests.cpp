// Tests for core/matching. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "matching.h"

TEST_CASE("matching stub is present", "[matching]") {
  CHECK(lumora::matching::kMaxSide == 65536u);
}

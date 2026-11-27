// Tests for core/hull2d. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "hull2d.h"

TEST_CASE("hull2d stub is present", "[hull2d]") {
  CHECK(lumora::hull2d::kMaxPoints == 65536u);
}

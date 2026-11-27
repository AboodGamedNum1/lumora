// Tests for core/dsu. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "dsu.h"

TEST_CASE("dsu stub is present", "[dsu]") {
  CHECK(lumora::dsu::kMaxElements == (1u << 26));
}

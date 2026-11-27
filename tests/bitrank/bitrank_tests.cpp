// Tests for core/bitrank. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "bitrank.h"

TEST_CASE("bitrank stub is present", "[bitrank]") {
  CHECK(lumora::bitrank::kMaxBits == (1u << 24));
}

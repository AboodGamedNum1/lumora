// Tests for core/bitset. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "bitset.h"

TEST_CASE("bitset stub is present", "[bitset]") {
  CHECK(lumora::bitset::kMaxBits == (1u << 30));
}

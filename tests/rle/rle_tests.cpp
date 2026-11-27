// Tests for core/rle. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "rle.h"

TEST_CASE("rle stub is present", "[rle]") {
  std::uint32_t n = 7;
  CHECK(lumora::rle::encode(nullptr, 0, nullptr, 0, nullptr) == lumora::rle::RleResult::NullArgument);
  CHECK(n == 7);
}

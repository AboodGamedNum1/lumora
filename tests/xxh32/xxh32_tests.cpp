// Tests for core/xxh32. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "xxh32.h"

TEST_CASE("xxh32 stub is present", "[xxh32]") {
  CHECK_FALSE(lumora::xxh32::hash(nullptr, 0, 0, nullptr));
}

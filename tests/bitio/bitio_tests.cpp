// Tests for core/bitio. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "bitio.h"

TEST_CASE("bitio stub is present", "[bitio]") {
  CHECK(lumora::bitio::bits_left(0, nullptr) == 0);
}

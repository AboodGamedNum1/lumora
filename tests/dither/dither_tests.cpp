// Tests for core/dither. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "dither.h"

TEST_CASE("dither stub is present", "[dither]") {
  CHECK_FALSE(lumora::dither::dither_row_u16(nullptr, nullptr, 1, 0, 0));
}

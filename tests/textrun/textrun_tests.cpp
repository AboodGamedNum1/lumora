// Tests for core/textrun. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "textrun.h"

TEST_CASE("textrun stub is present", "[textrun]") {
  std::uint32_t n = 0;
  CHECK_FALSE(lumora::textrun::count_code_points(nullptr, 1, &n));
}

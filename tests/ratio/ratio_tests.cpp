// Tests for core/ratio. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "ratio.h"

TEST_CASE("ratio stub is present", "[ratio]") {
  lumora::ratio::Ratio r{0, 1};
  CHECK(r.den == 1);
}

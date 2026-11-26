// Tests for core/boxfilter. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "box_filter.h"

TEST_CASE("box filter stub is present", "[boxfilter]") {
  CHECK(lumora::boxfilter::kMaxFactor == 16u);
}

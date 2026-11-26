// Tests for core/lifecycle/frame_stats. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "frame_stats.h"

TEST_CASE("frame_stats stub is present", "[lifecycle]") {
  using namespace lumora::lifecycle;
  CHECK_FALSE(record_frame(nullptr, 1));
}

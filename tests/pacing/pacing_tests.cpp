// Tests for core/lifecycle/pacing. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "pacing.h"

TEST_CASE("pacing stub is present", "[lifecycle]") {
  using namespace lumora::lifecycle;
  CHECK_FALSE(on_thermal_throttle(nullptr, ScaleConfig{}, nullptr));
}

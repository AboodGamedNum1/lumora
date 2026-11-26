// Tests for core/lifecycle/power_policy. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "power_policy.h"

TEST_CASE("power_policy stub is present", "[lifecycle]") {
  using namespace lumora::lifecycle;
  CHECK_FALSE(render_scale_percent(0, 3, nullptr));
}

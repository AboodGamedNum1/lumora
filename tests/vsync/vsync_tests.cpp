// Tests for core/lifecycle/vsync. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "vsync.h"

TEST_CASE("vsync stub is present", "[lifecycle]") {
  using namespace lumora::lifecycle;
  CHECK_FALSE(on_vsync(nullptr, 1));
}

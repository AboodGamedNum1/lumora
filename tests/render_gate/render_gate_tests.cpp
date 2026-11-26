// Tests for core/lifecycle/render_gate. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "render_gate.h"

TEST_CASE("render_gate stub is present", "[lifecycle]") {
  using namespace lumora::lifecycle;
  CHECK_FALSE(apply_event(nullptr, GateEvent::ScreenOn, nullptr));
}

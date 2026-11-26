// Tests for core/lifecycle/mic_state. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "mic_state.h"

TEST_CASE("mic state stub is present", "[lifecycle]") {
  using namespace lumora::lifecycle;
  CHECK(next_state(MicState::Inactive, MicEvent::Visible) == MicState::Inactive);
}

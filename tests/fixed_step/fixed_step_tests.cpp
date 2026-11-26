// Tests for core/timing/fixed_step. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "fixed_step.h"

TEST_CASE("fixed step stub is present", "[timing]") {
  lumora::timing::StepConfig c;
  CHECK_FALSE(lumora::timing::advance(nullptr, 1, c, nullptr));
}

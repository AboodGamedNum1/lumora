// Tests for core/assign. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "assign.h"

TEST_CASE("assign stub is present", "[assign]") {
  CHECK(lumora::assign::kMaxN == 512u);
}

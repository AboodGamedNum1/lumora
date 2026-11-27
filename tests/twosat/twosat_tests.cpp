// Tests for core/twosat. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "twosat.h"

TEST_CASE("twosat stub is present", "[twosat]") {
  CHECK(lumora::twosat::kMaxVars == 65536u);
}

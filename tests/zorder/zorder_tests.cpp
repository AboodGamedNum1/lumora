// Tests for core/zorder. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "zorder.h"

TEST_CASE("zorder stub is present", "[zorder]") {
  CHECK(lumora::zorder::kMaxHilbertOrder == 16u);
}

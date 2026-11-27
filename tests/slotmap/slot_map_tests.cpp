// Tests for core/slotmap. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "slot_map.h"

TEST_CASE("slot map stub is present", "[slotmap]") {
  CHECK(lumora::slotmap::size(nullptr) == 0);
}

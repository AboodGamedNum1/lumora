// Tests for core/damage. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "damage.h"

TEST_CASE("damage stub is present", "[damage]") {
  lumora::damage::DamageSet s;
  CHECK(lumora::damage::size(&s) == 0);
}

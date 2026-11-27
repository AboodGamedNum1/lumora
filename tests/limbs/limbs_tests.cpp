// Tests for core/limbs. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "limbs.h"

TEST_CASE("limbs stub is present", "[limbs]") {
  CHECK(lumora::limbs::kMaxLimbs == (1u << 16));
}

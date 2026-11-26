// Tests for core/prng. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "prng.h"

TEST_CASE("prng stub is present", "[prng]") {
  CHECK_FALSE(lumora::prng::pcg32_seed(nullptr, 1, 1));
}

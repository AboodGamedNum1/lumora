// Tests for core/palin. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "palin.h"

TEST_CASE("palin stub is present", "[palin]") {
  CHECK(lumora::palin::kMaxLen == 262144u);
}

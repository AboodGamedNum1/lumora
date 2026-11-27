// Tests for core/suffix. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "suffix.h"

TEST_CASE("suffix stub is present", "[suffix]") {
  CHECK(lumora::suffix::kMaxLen == (1u << 16));
}

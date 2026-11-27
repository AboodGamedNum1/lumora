// Tests for core/sortedops. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "sortedops.h"

TEST_CASE("sortedops stub is present", "[sortedops]") {
  CHECK(lumora::sortedops::kMaxCount == (1u << 26));
}

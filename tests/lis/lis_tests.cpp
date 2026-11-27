// Tests for core/lis. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "lis.h"

TEST_CASE("lis stub is present", "[lis]") {
  CHECK(lumora::lis::kMaxLen == 262144u);
}

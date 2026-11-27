// Tests for core/lyndon. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "lyndon.h"

TEST_CASE("lyndon stub is present", "[lyndon]") {
  CHECK(lumora::lyndon::kMaxLen == 262144u);
}

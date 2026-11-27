// Tests for core/bridges. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "bridges.h"

TEST_CASE("bridges stub is present", "[bridges]") {
  CHECK(lumora::bridges::kMaxNodes == 65536u);
}

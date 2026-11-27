// Tests for core/toposort. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "toposort.h"

TEST_CASE("toposort stub is present", "[toposort]") {
  CHECK(lumora::toposort::kMaxNodes == (1u << 16));
}

// Tests for core/sssp. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "sssp.h"

TEST_CASE("sssp stub is present", "[sssp]") {
  CHECK(lumora::sssp::kMaxNodes == 65536u);
}

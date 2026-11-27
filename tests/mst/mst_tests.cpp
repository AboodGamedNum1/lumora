// Tests for core/mst. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "mst.h"

TEST_CASE("mst stub is present", "[mst]") {
  CHECK(lumora::mst::kMaxNodes == 65536u);
}

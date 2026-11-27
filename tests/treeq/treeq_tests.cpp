// Tests for core/treeq. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "treeq.h"

TEST_CASE("treeq stub is present", "[treeq]") {
  CHECK(lumora::treeq::kMaxNodes == 65536u);
}

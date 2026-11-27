// Tests for core/minheap. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "minheap.h"

TEST_CASE("minheap stub is present", "[minheap]") {
  CHECK(lumora::minheap::kMaxCapacity == (1u << 26));
}

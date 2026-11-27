// Tests for core/ivset. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "ivset.h"

TEST_CASE("ivset stub is present", "[ivset]") {
  CHECK(lumora::ivset::kMaxCapacity == (1u << 24));
}

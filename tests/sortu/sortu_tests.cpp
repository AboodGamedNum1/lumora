// Tests for core/sortu. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "sortu.h"

TEST_CASE("sortu stub is present", "[sortu]") {
  CHECK(lumora::sortu::kMaxCount == (1u << 26));
}

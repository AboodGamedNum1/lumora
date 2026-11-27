// Tests for core/globmatch. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "globmatch.h"

TEST_CASE("globmatch stub is present", "[globmatch]") {
  CHECK(lumora::globmatch::kMaxLen == (1u << 16));
}

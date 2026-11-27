// Tests for core/seqdist. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "seqdist.h"

TEST_CASE("seqdist stub is present", "[seqdist]") {
  CHECK(lumora::seqdist::kMaxLen == (1u << 16));
}

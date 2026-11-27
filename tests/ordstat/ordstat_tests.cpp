// Tests for core/ordstat. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "ordstat.h"

TEST_CASE("ordstat stub is present", "[ordstat]") {
  CHECK(lumora::ordstat::kMaxLen == (1u << 20));
}

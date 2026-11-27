// Tests for core/perm. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "perm.h"

TEST_CASE("perm stub is present", "[perm]") {
  CHECK(lumora::perm::kMaxRankN == 20u);
}

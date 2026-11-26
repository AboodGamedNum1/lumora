// Tests for core/linebreak. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "linebreak.h"

TEST_CASE("linebreak stub is present", "[linebreak]") {
  CHECK(lumora::linebreak::kMaxItems == 65536u);
}

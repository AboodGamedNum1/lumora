// Tests for core/euler. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "euler.h"

TEST_CASE("euler stub is present", "[euler]") {
  CHECK(lumora::euler::kMaxNodes == 65536u);
}

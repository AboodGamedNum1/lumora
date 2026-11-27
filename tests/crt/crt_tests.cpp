// Tests for core/crt. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "crt.h"

TEST_CASE("crt stub is present", "[crt]") {
  CHECK(lumora::crt::kMaxEquations == 64u);
}

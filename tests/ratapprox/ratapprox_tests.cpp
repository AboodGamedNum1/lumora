// Tests for core/ratapprox. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "ratapprox.h"

TEST_CASE("ratapprox stub is present", "[ratapprox]") {
  lumora::ratapprox::Frac f{0, 1};
  CHECK(f.den == 1);
}

// Tests for core/ring. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "byte_ring.h"

TEST_CASE("byte ring stub is present", "[ring]") {
  lumora::ring::ByteRing r;
  CHECK(lumora::ring::size(&r) == 0);
}

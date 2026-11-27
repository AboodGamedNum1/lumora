// Tests for core/hex. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "hex.h"

TEST_CASE("hex stub is present", "[hex]") {
  CHECK(lumora::hex::kMaxInput == (1u << 27));
}

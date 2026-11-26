// Tests for core/varint. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "varint.h"

TEST_CASE("varint stub is present", "[varint]") {
  CHECK(lumora::varint::kMaxBytes == 10);
}

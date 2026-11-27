// Tests for core/decfmt. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "decfmt.h"

TEST_CASE("decfmt stub is present", "[decfmt]") {
  CHECK(lumora::decfmt::kMaxParseLen == (1u << 20));
}

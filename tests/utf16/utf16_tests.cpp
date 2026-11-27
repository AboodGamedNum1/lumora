// Tests for core/utf16. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "utf16.h"

TEST_CASE("utf16 stub is present", "[utf16]") {
  CHECK(lumora::utf16::kMaxCodePoint == 0x10FFFFu);
}

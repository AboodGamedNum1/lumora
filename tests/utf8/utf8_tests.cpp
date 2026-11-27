// Tests for core/utf8. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "utf8.h"

TEST_CASE("utf8 stub is present", "[utf8]") {
  CHECK(lumora::utf8::kMaxCodePoint == 0x10FFFFu);
}

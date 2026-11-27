// Tests for core/bytesearch. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "bytesearch.h"

TEST_CASE("bytesearch stub is present", "[bytesearch]") {
  CHECK(lumora::bytesearch::kMaxLen == (1u << 28));
}

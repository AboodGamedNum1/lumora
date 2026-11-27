// Tests for core/checksum. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "checksum.h"

TEST_CASE("checksum stub is present", "[checksum]") {
  CHECK(lumora::checksum::crc32(nullptr, 0) == 0);
}

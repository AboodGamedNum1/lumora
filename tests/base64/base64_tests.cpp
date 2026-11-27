// Tests for core/base64. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "base64.h"

TEST_CASE("base64 stub is present", "[base64]") {
  CHECK(lumora::base64::encode(nullptr, 0, nullptr, 0, nullptr) == lumora::base64::Base64Result::NullArgument);
}

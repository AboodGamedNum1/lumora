// Placeholder. The capabilities contributor replaces this file with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "capabilities.h"

TEST_CASE("capabilities stub links", "[capabilities]") {
  REQUIRE(lumora::package::capability_bit(lumora::package::Capability::Notifications) == 2u);
}

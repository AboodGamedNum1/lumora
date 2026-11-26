// Placeholder. The zip audit contributor replaces this file with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "zip_audit.h"

TEST_CASE("zip audit stub links", "[zip_audit]") {
  REQUIRE(lumora::package::ZipAuditLimits{}.max_entries == 10000);
}

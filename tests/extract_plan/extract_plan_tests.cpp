// Placeholder. The extraction plan contributor replaces this file with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "extract_plan.h"

TEST_CASE("extract plan stub links", "[extract_plan]") {
  REQUIRE(lumora::package::ExtractLimits{}.max_depth == 16);
}

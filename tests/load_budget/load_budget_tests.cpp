// Placeholder. The load budget contributor replaces this file with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "load_budget.h"

TEST_CASE("load budget stub links", "[load_budget]") {
  REQUIRE(lumora::package::LoadBudgetLimits{}.max_nodes == 100000);
}

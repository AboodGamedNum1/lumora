#include <catch2/catch_test_macros.hpp>
#include "grid.h"

TEST_CASE("grid stub links", "[grid]") { REQUIRE(lumora::grid::grid_status_text(lumora::grid::GridStatus::Ok) != nullptr); }

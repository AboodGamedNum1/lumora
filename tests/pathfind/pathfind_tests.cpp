#include <catch2/catch_test_macros.hpp>
#include "pathfind.h"

TEST_CASE("pathfind stub links", "[pathfind]") { REQUIRE(lumora::pathfind::map_status_text(lumora::pathfind::MapStatus::Ok) != nullptr); }

#include <catch2/catch_test_macros.hpp>
#include "geom.h"

TEST_CASE("geom stub links", "[geom]") { REQUIRE(lumora::geom::poly_status_text(lumora::geom::PolyStatus::Ok) != nullptr); }

#include <catch2/catch_test_macros.hpp>
#include "spline.h"

TEST_CASE("spline stub links", "[spline]") { REQUIRE(lumora::spline::path_status_text(lumora::spline::PathStatus::Ok) != nullptr); }

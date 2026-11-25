#include <catch2/catch_test_macros.hpp>
#include "color.h"

TEST_CASE("color stub links", "[color]") { REQUIRE(lumora::color::gradient_status_text(lumora::color::GradientStatus::Ok) != nullptr); }

#include <catch2/catch_test_macros.hpp>
#include "textlayout.h"

TEST_CASE("textlayout stub links", "[textlayout]") { REQUIRE(lumora::textlayout::font_status_text(lumora::textlayout::FontStatus::Ok) != nullptr); }

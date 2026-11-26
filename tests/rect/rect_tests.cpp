// Tests for core/rect. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "rect.h"

TEST_CASE("rect stub is present", "[rect]") {
  CHECK(lumora::rect::is_empty(lumora::rect::Rect{}));
}

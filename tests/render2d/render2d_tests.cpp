// Placeholder owned by the render2d contributor. Replace with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "cpu_renderer.h"

TEST_CASE("render2d stub builds", "[render2d]") {
  lumora::render2d::Image i;
  CHECK_FALSE(i.valid());
}

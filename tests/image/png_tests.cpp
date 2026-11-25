// Placeholder: the PNG contributor replaces this file with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "png.h"

TEST_CASE("png interface placeholder: an empty input is not a png", "[image]") {
  const auto r = lumora::image::decode_png(nullptr, 0);
  CHECK_FALSE(r.ok());
}

// Tests for core/glyphblit. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "glyph_blit.h"

TEST_CASE("glyph blit stub is present", "[glyphblit]") {
  CHECK(lumora::glyphblit::kMaxDim == 4096);
}

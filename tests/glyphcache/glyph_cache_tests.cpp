// Tests for core/glyphcache. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "glyph_cache.h"

TEST_CASE("glyph cache stub is present", "[glyphcache]") {
  CHECK_FALSE(lumora::glyphcache::cache_clear(nullptr));
}

// Tests for core/audio/analysis. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "analysis.h"

TEST_CASE("audio analysis stub is present", "[audio]") {
  lumora::audio::Level l;
  CHECK_FALSE(lumora::audio::measure_level(nullptr, 1, &l));
}

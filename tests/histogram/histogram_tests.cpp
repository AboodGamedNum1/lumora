// Tests for core/histogram. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "histogram.h"

TEST_CASE("histogram stub is present", "[histogram]") {
  lumora::histogram::Histogram h;
  CHECK(lumora::histogram::total(&h) == 0);
}

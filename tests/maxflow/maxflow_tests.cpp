// Tests for core/maxflow. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "maxflow.h"

TEST_CASE("maxflow stub is present", "[maxflow]") {
  CHECK(lumora::maxflow::kMaxNodes == 4096u);
}

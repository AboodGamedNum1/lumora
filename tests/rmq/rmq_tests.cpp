// Tests for core/rmq. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "rmq.h"

TEST_CASE("rmq stub is present", "[rmq]") {
  CHECK(lumora::rmq::kMaxLen == (1u << 16));
}

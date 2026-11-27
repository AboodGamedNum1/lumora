// Tests for core/combin. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "combin.h"

TEST_CASE("combin stub is present", "[combin]") {
  CHECK(static_cast<int>(lumora::combin::CombResult::Ok) == 0);
}

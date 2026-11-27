// Tests for core/scc. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "scc.h"

TEST_CASE("scc stub is present", "[scc]") {
  CHECK(lumora::scc::kMaxNodes == 65536u);
}

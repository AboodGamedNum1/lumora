// Tests for core/lifecycle/crash_breaker. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "crash_breaker.h"

TEST_CASE("crash breaker stub is present", "[lifecycle]") {
  lumora::lifecycle::Policy p;
  CHECK_FALSE(lumora::lifecycle::record_crash(nullptr, 1, p));
}

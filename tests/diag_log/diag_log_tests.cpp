// Tests for core/diag/diag_log. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "diag_log.h"

TEST_CASE("diag_log stub is present", "[diag]") {
  CHECK_FALSE(lumora::diag::log_clear(nullptr));
}

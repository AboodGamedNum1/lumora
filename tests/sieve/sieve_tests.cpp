// Tests for core/sieve. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "sieve.h"

TEST_CASE("sieve stub is present", "[sieve]") {
  CHECK(lumora::sieve::kMaxLimit == 1048576u);
}

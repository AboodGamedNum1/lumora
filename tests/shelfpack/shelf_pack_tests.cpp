// Tests for core/shelfpack. Evidence label: unit tests only. Placeholder until the implementation lands.
#include <catch2/catch_test_macros.hpp>

#include "shelf_pack.h"

TEST_CASE("shelf pack stub is present", "[shelfpack]") {
  CHECK_FALSE(lumora::shelfpack::packer_init(nullptr, 1, 1));
}

// Placeholder test for the easing module. The contributor replaces this file.
#include <catch2/catch_test_macros.hpp>

#include "easing.h"

TEST_CASE("easing interface placeholder: an unknown name is rejected", "[anim][easing]") {
  lumora::anim::Ease e;
  CHECK_FALSE(lumora::anim::ease_from_name("no-such-curve", e));
}

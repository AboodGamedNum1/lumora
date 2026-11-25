// Placeholder test for the animation module. The contributor replaces this file.
#include <catch2/catch_test_macros.hpp>

#include "animation.h"

TEST_CASE("animation interface placeholder: an empty track is not valid", "[anim]") {
  CHECK(lumora::anim::validate_track({}) != lumora::anim::TrackStatus::Ok);
}

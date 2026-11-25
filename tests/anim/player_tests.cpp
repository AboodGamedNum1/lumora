// Placeholder test for the clip player. The contributor replaces this file.
#include <catch2/catch_test_macros.hpp>

#include "player.h"

TEST_CASE("player interface placeholder: an empty clip does not bind yet", "[anim][player]") {
  lumora::scene::Scene s;
  lumora::anim::Clip c;
  CHECK_FALSE(lumora::anim::bind_clip(s, c).ok());
}

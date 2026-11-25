// Placeholder test for the particle module. The contributor replaces this file.
#include <catch2/catch_test_macros.hpp>

#include "particles.h"

TEST_CASE("particle interface placeholder: init is not available yet", "[particles]") {
  lumora::particles::Emitter e;
  CHECK_FALSE(lumora::particles::emitter_init({}, e));
}

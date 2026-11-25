// Placeholder test for the atlas packer. The contributor replaces this file.
#include <catch2/catch_test_macros.hpp>

#include "atlas.h"

TEST_CASE("atlas interface placeholder: no inputs is not ok", "[atlas]") {
  CHECK_FALSE(lumora::image::pack_atlas({}).ok());
}

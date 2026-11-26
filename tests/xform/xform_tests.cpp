#include <catch2/catch_test_macros.hpp>
#include "xform.h"

TEST_CASE("xform stub links", "[xform]") { REQUIRE(lumora::xform::camera_status_text(lumora::xform::CameraStatus::Ok) != nullptr); }

// Placeholder owned by the settings contributor. Replace with the real tests.
#include <catch2/catch_test_macros.hpp>

#include "settings.h"

TEST_CASE("settings stub builds", "[settings]") {
  CHECK_FALSE(lumora::scene::parse_settings_schema("{}").ok());
}

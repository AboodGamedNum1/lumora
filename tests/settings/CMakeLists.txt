# Tests for core/scene/settings. Pure CPU. Owned by the settings contributor.
add_executable(lumora_settings_tests settings_tests.cpp)
target_link_libraries(lumora_settings_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_settings_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/scene)
lumora_target_warnings(lumora_settings_tests)
add_test(NAME lumora_settings_tests COMMAND lumora_settings_tests)

# Tests for the damage module (core/damage). Pure CPU. Owned by the damage contributor.
add_executable(lumora_damage_tests damage_tests.cpp)
target_link_libraries(lumora_damage_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_damage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/damage ${CMAKE_CURRENT_SOURCE_DIR}/../../core/rect)
lumora_target_warnings(lumora_damage_tests)
add_test(NAME lumora_damage_tests COMMAND lumora_damage_tests)

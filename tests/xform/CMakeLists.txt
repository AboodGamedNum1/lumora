# Tests for the particle module (core/xform). Pure CPU. Owned by the xform contributor.
add_executable(lumora_xform_tests xform_tests.cpp)
target_link_libraries(lumora_xform_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_xform_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/xform)
lumora_target_warnings(lumora_xform_tests)
add_test(NAME lumora_xform_tests COMMAND lumora_xform_tests)

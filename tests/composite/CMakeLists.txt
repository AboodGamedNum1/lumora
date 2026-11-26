# Tests for the compositing module (core/composite). Pure CPU. Owned by the composite contributor.
add_executable(lumora_composite_tests composite_tests.cpp)
target_link_libraries(lumora_composite_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_composite_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/composite)
lumora_target_warnings(lumora_composite_tests)
add_test(NAME lumora_composite_tests COMMAND lumora_composite_tests)

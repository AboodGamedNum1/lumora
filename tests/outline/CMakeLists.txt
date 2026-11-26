# Tests for the outline builder (core/outline). Pure CPU. Owned by the outline contributor.
add_executable(lumora_outline_tests outline_tests.cpp)
target_link_libraries(lumora_outline_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_outline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/outline)
lumora_target_warnings(lumora_outline_tests)
add_test(NAME lumora_outline_tests COMMAND lumora_outline_tests)
target_include_directories(lumora_outline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/flatten)

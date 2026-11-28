# Tests for the farey module (core/farey). Pure CPU. Owned by the farey contributor.
add_executable(lumora_farey_tests farey_tests.cpp)
target_link_libraries(lumora_farey_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_farey_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/farey)
lumora_target_warnings(lumora_farey_tests)
add_test(NAME lumora_farey_tests COMMAND lumora_farey_tests)

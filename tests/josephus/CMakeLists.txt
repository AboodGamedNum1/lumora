# Tests for the josephus module (core/josephus). Pure CPU. Owned by the josephus contributor.
add_executable(lumora_josephus_tests josephus_tests.cpp)
target_link_libraries(lumora_josephus_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_josephus_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/josephus)
lumora_target_warnings(lumora_josephus_tests)
add_test(NAME lumora_josephus_tests COMMAND lumora_josephus_tests)

# Tests for the ivset module (core/ivset). Pure CPU. Owned by the ivset contributor.
add_executable(lumora_ivset_tests ivset_tests.cpp)
target_link_libraries(lumora_ivset_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ivset_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ivset)
lumora_target_warnings(lumora_ivset_tests)
add_test(NAME lumora_ivset_tests COMMAND lumora_ivset_tests)

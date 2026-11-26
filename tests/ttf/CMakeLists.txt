# Tests for the TrueType reader (core/ttf). Pure CPU. Owned by the ttf contributor.
add_executable(lumora_ttf_tests ttf_tests.cpp)
target_link_libraries(lumora_ttf_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ttf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ttf)
lumora_target_warnings(lumora_ttf_tests)
add_test(NAME lumora_ttf_tests COMMAND lumora_ttf_tests)
target_include_directories(lumora_ttf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/flatten ${CMAKE_CURRENT_SOURCE_DIR}/../../core/outline)

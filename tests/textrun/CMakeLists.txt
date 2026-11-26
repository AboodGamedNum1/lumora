# Tests for the text run (core/textrun). Pure CPU. Owned by the textrun contributor.
add_executable(lumora_textrun_tests textrun_tests.cpp)
target_link_libraries(lumora_textrun_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_textrun_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/textrun ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ttf)
target_include_directories(lumora_textrun_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/flatten ${CMAKE_CURRENT_SOURCE_DIR}/../../core/outline)
lumora_target_warnings(lumora_textrun_tests)
add_test(NAME lumora_textrun_tests COMMAND lumora_textrun_tests)

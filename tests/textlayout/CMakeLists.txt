# Tests for the text layout module (core/textlayout). Pure CPU. Owned by the textlayout contributor.
add_executable(lumora_textlayout_tests textlayout_tests.cpp)
target_link_libraries(lumora_textlayout_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_textlayout_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/textlayout)
lumora_target_warnings(lumora_textlayout_tests)
add_test(NAME lumora_textlayout_tests COMMAND lumora_textlayout_tests)

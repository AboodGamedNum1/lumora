# Tests for the glyph blit (core/glyphblit). Pure CPU. Owned by the glyph blit contributor.
add_executable(lumora_glyphblit_tests glyph_blit_tests.cpp)
target_link_libraries(lumora_glyphblit_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_glyphblit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/glyphblit)
lumora_target_warnings(lumora_glyphblit_tests)
add_test(NAME lumora_glyphblit_tests COMMAND lumora_glyphblit_tests)

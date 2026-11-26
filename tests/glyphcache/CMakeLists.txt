# Tests for the glyph cache index (core/glyphcache). Pure CPU. Owned by the glyph cache contributor.
add_executable(lumora_glyphcache_tests glyph_cache_tests.cpp)
target_link_libraries(lumora_glyphcache_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_glyphcache_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/glyphcache)
lumora_target_warnings(lumora_glyphcache_tests)
add_test(NAME lumora_glyphcache_tests COMMAND lumora_glyphcache_tests)

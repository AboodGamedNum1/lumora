# Tests for the shelf allocator (core/shelfpack). Pure CPU. Owned by the shelf allocator contributor.
add_executable(lumora_shelfpack_tests shelf_pack_tests.cpp)
target_link_libraries(lumora_shelfpack_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_shelfpack_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/shelfpack)
lumora_target_warnings(lumora_shelfpack_tests)
add_test(NAME lumora_shelfpack_tests COMMAND lumora_shelfpack_tests)

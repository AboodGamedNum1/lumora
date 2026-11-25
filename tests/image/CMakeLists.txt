# Tests for core/image: the PNG codec. Pure CPU. Owned by the PNG contributor.
add_executable(lumora_image_tests png_tests.cpp)
target_link_libraries(lumora_image_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_image_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/image ${CMAKE_CURRENT_SOURCE_DIR}/../../core/package)
lumora_target_warnings(lumora_image_tests)
add_test(NAME lumora_image_tests COMMAND lumora_image_tests)

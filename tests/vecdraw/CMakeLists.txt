# Tests for the vector path drawing (core/vecdraw). Pure CPU. Owned by the vecdraw contributor.
add_executable(lumora_vecdraw_tests vecdraw_tests.cpp)
target_link_libraries(lumora_vecdraw_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_vecdraw_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/vecdraw)
lumora_target_warnings(lumora_vecdraw_tests)
add_test(NAME lumora_vecdraw_tests COMMAND lumora_vecdraw_tests)
target_include_directories(lumora_vecdraw_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/flatten ${CMAKE_CURRENT_SOURCE_DIR}/../../core/outline ${CMAKE_CURRENT_SOURCE_DIR}/../../core/stroke ${CMAKE_CURRENT_SOURCE_DIR}/../../core/raster)

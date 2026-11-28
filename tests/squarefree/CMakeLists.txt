# Tests for the squarefree module (core/squarefree). Pure CPU. Owned by the squarefree contributor.
add_executable(lumora_squarefree_tests squarefree_tests.cpp)
target_link_libraries(lumora_squarefree_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_squarefree_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/squarefree)
lumora_target_warnings(lumora_squarefree_tests)
add_test(NAME lumora_squarefree_tests COMMAND lumora_squarefree_tests)

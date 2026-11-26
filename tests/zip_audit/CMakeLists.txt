# Tests for the ZIP container audit (core/package/zip_audit). Pure CPU. Owned by the zip audit contributor.
add_executable(lumora_zip_audit_tests zip_audit_tests.cpp)
target_link_libraries(lumora_zip_audit_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_zip_audit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/package)
lumora_target_warnings(lumora_zip_audit_tests)
add_test(NAME lumora_zip_audit_tests COMMAND lumora_zip_audit_tests)

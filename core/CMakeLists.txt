add_library(lumora_core STATIC src/version.cpp)
target_include_directories(lumora_core PUBLIC include)
target_compile_features(lumora_core PUBLIC cxx_std_23)
target_compile_definitions(lumora_core PUBLIC
  LUMORA_VERSION_STRING="${PROJECT_VERSION}")
lumora_target_warnings(lumora_core)

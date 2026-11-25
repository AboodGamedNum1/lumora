cmake_minimum_required(VERSION 3.22)
project(Lumora VERSION 0.0.1 LANGUAGES CXX)

# Language level: the plan targets C++26 where the pinned toolchains support it.
# Until docs/cpp-standard.md says a feature is available on every pinned compiler,
# the code is built as C++23. Override with -DLUMORA_CXX_STANDARD=26 to try.
set(LUMORA_CXX_STANDARD 23 CACHE STRING "C++ standard used for all Lumora targets")
set(CMAKE_CXX_STANDARD ${LUMORA_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LUMORA_BUILD_TESTS "Build unit tests" ON)
option(LUMORA_BUILD_EDITOR "Build the desktop editor" ON)
option(LUMORA_WIN_VULKAN_SPIKE "Windows CI spike: build the Vulkan backend against the system loader" OFF)
option(LUMORA_SANITIZE "Enable ASan and UBSan (GCC/Clang)" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(LumoraWarnings)

if(LUMORA_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(core)

if(LUMORA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

add_subdirectory(spikes)

if(LUMORA_BUILD_EDITOR)
  add_subdirectory(editor)
endif()

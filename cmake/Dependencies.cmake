# Pinned dependencies. Versions and licenses are recorded in docs/dependency-manifest.md.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

function(lumora_fetch_catch2)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.9.1
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(Catch2)
endfunction()

function(lumora_fetch_sdl3)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.20
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(SDL3)
endfunction()

function(lumora_fetch_imgui)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b-docking
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(imgui)
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
endfunction()

# SPIRV-Cross, pinned to the Vulkan SDK tag recorded in docs/dependency-manifest.md. Only the GLSL backend is built.
function(lumora_fetch_spirv_cross)
  set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SPIRV-Cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG vulkan-sdk-1.4.357.0
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(SPIRV-Cross)
endfunction()

# Lua 5.4.9 (MIT). The release tarball is verified against the SHA-256 that lua.org publishes at
# https://www.lua.org/ftp/ . It is built as a static library from source, as C++ so that Lua errors are C++
# exceptions and unwind our frames safely. Only the core and the base, math, string and table libraries are
# compiled: io, os, package, debug, coroutine and utf8 are not part of the build at all.
function(lumora_fetch_lua)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  FetchContent_Declare(lua
    URL https://www.lua.org/ftp/lua-5.4.9.tar.gz
    URL_HASH SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6)
  FetchContent_MakeAvailable(lua)
  set(_names lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable
             ltm lundump lvm lzio lauxlib lbaselib lmathlib lstrlib ltablib)
  set(_files "")
  foreach(n ${_names})
    list(APPEND _files ${lua_SOURCE_DIR}/src/${n}.c)
  endforeach()
  add_library(lumora_lua STATIC ${_files})
  set_source_files_properties(${_files} PROPERTIES LANGUAGE CXX)
  target_include_directories(lumora_lua PUBLIC ${lua_SOURCE_DIR}/src)
  target_compile_features(lumora_lua PUBLIC cxx_std_17)
  if(MSVC)
    target_compile_options(lumora_lua PRIVATE /W0 /TP)
  else()
    target_compile_options(lumora_lua PRIVATE -w)
  endif()
  set_target_properties(lumora_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

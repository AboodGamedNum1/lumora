add_library(lumora_core STATIC src/version.cpp rhi/src/rhi.cpp)
target_include_directories(lumora_core PUBLIC include rhi/include)
target_compile_features(lumora_core PUBLIC cxx_std_23)
target_compile_definitions(lumora_core PUBLIC
  LUMORA_VERSION_STRING="${PROJECT_VERSION}")
lumora_target_warnings(lumora_core)

# Vulkan backend: built when the Vulkan headers and loader are found (Linux CI installs them).
option(LUMORA_ENABLE_VULKAN "Build the Vulkan RHI backend when Vulkan is available" ON)
if(LUMORA_ENABLE_VULKAN)
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    target_sources(lumora_core PRIVATE rhi/vulkan/vulkan_device.cpp)
    if(NOT MSVC)
      # Vulkan structs are initialised with only sType set, the rest is zero by design.
      set_source_files_properties(rhi/vulkan/vulkan_device.cpp PROPERTIES COMPILE_OPTIONS -Wno-missing-field-initializers)
    endif()
    target_link_libraries(lumora_core PUBLIC Vulkan::Vulkan)
    target_compile_definitions(lumora_core PUBLIC LUMORA_HAS_VULKAN=1)
    message(STATUS "Lumora: Vulkan backend enabled")
  else()
    message(STATUS "Lumora: Vulkan not found, backend disabled")
  endif()
endif()

# D3D12 backend: Windows only. Its runtime evidence in CI is the WARP software adapter.
if(WIN32)
  target_sources(lumora_core PRIVATE rhi/d3d12/d3d12_device.cpp)
  target_link_libraries(lumora_core PUBLIC d3d12 dxgi)
  target_compile_definitions(lumora_core PUBLIC LUMORA_HAS_D3D12=1)
endif()

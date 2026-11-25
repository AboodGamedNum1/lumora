# Spike only (option LUMORA_WIN_VULKAN_SPIKE): lets the Vulkan backend build on a Windows machine that
# has the Vulkan loader (vulkan-1.dll) but no Vulkan SDK, such as a CI runner. It fetches the pinned
# Vulkan headers and builds an import library for the system loader from its exported names.
# Not used for shipping builds.
include(FetchContent)

function(lumora_windows_vulkan_spike)
  FetchContent_Declare(VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG vulkan-sdk-1.4.357.0
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(VulkanHeaders)

  set(dll "$ENV{windir}/System32/vulkan-1.dll")
  get_filename_component(linker_dir "${CMAKE_LINKER}" DIRECTORY)
  find_program(DUMPBIN dumpbin HINTS "${linker_dir}")
  if(NOT DUMPBIN OR NOT EXISTS "${dll}")
    message(STATUS "Lumora: Windows Vulkan spike: dumpbin or vulkan-1.dll missing")
    return()
  endif()
  execute_process(COMMAND "${DUMPBIN}" /exports "${dll}" OUTPUT_VARIABLE exports)
  string(REGEX MATCHALL "[ \t]vk[A-Za-z0-9_]+" names "${exports}")
  set(def "LIBRARY vulkan-1\nEXPORTS\n")
  foreach(n ${names})
    string(STRIP "${n}" n)
    string(APPEND def "  ${n}\n")
  endforeach()
  set(dir "${CMAKE_BINARY_DIR}/vulkan-import")
  file(MAKE_DIRECTORY "${dir}")
  file(WRITE "${dir}/vulkan-1.def" "${def}")
  get_filename_component(lib_tool "${CMAKE_AR}" ABSOLUTE)
  execute_process(COMMAND "${CMAKE_AR}" /nologo /def:${dir}/vulkan-1.def /out:${dir}/vulkan-1.lib /machine:x64
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0 OR NOT EXISTS "${dir}/vulkan-1.lib")
    message(STATUS "Lumora: Windows Vulkan spike: import library failed: ${out}")
    return()
  endif()
  add_library(lumora_vulkan_loader STATIC IMPORTED GLOBAL)
  set_target_properties(lumora_vulkan_loader PROPERTIES IMPORTED_LOCATION "${dir}/vulkan-1.lib")
  target_link_libraries(lumora_vulkan_loader INTERFACE Vulkan::Headers)
  set(LUMORA_WIN_VULKAN_OK ON PARENT_SCOPE)
endfunction()

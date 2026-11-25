if(WIN32)
  add_executable(warp_spike warp/warp_spike.cpp)
  target_link_libraries(warp_spike PRIVATE d3d12 dxgi)
endif()

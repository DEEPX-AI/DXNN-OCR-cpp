# DXRT Test Examples (run async test)
set(target dxrt_test_runasync_wait)
file(GLOB_RECURSE srcs "*.cpp" "*.cc" "*.c")
 
add_executable(${target} ${srcs})
target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)  
add_dxrt(${target})
install( TARGETS ${target} DESTINATION bin/test )
install( TARGETS ${target} DESTINATION ${CMAKE_SOURCE_DIR}/bin/test )
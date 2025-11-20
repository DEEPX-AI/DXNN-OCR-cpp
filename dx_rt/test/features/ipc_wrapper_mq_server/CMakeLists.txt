# DXRT Test Examples
set(target dxrt_test_ipc_wrapper_mq_server)
file(GLOB_RECURSE srcs "*.cpp" "*.cc" "*.c")
 
add_executable(${target} ${srcs})
target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)  
add_dxrt(${target})
#add_googletest(${target})
install( TARGETS ${target} DESTINATION bin/test )
install( TARGETS ${target} DESTINATION ${CMAKE_SOURCE_DIR}/bin/test )

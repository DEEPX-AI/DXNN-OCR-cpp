# DXRT Test
set(target dxrt_test)
if(USE_DXRT_TEST)

  # FILE(GLOB srcs "*.cpp" "*.cc" "*.c")
  # add_executable(${target} ${srcs})
  add_subdirectory(unittest)
  add_subdirectory(features)
  add_subdirectory(release/validation_test/test_app/cpp)
  if(MSVC)
    # link runtime_settings to target
    target_link_libraries(${target} PUBLIC runtime_settings)
  endif()
  target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)  
  add_dxrt(${target})
  add_googletest(${target})

  install( TARGETS ${target} DESTINATION bin )
  install( TARGETS ${target} DESTINATION ${CMAKE_SOURCE_DIR}/bin )

  if(MSVC)
    get_target_property(GTEST_DLL GTest::gtest LOCATION)
    get_target_property(GMOCK_DLL GTest::gmock LOCATION)
    get_target_property(GTEST_MAIN_DLL GTest::gtest_main LOCATION)
    get_target_property(GMOCK_MAIN_DLL GTest::gmock_main LOCATION)

    message(STATUS "GTest DLL: ${GTEST_DLL}")
    message(STATUS "GMock DLL: ${GMOCK_DLL}")
    message(STATUS "GMock_main DLL: ${GTEST_MAIN_DLL}")
    message(STATUS "GMock_main DLL: ${GMOCK_MAIN_DLL}")

    install(FILES 
      ${GTEST_DLL} 
      ${GMOCK_DLL} 
      ${GTEST_MAIN_DLL} 
      ${GMOCK_MAIN_DLL}
      DESTINATION bin
    )
  endif()
endif()

# if(USE_DXRT_TEST)
  # add_subdirectory(unittest)
  # add_subdirectory(features)
# endif()
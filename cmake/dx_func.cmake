set(DXRT_INSTALLED_DIR /usr/local CACHE PATH "DXRT installation directory")

macro(add_dxrt_lib)
    find_package(dxrt REQUIRED HINTS ${DXRT_INSTALLED_DIR})
    list(APPEND link_libs dxrt pthread)
endmacro(add_dxrt_lib)

macro(add_target name)
    target_include_directories(${name} PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )
    
    target_link_libraries(${name} 
        ${link_libs} 
        ${OpenCV_LIBS}
        stdc++fs
    )

    install(
        TARGETS ${name}
        DESTINATION bin
        LIBRARY DESTINATION lib
    )
endmacro(add_target)

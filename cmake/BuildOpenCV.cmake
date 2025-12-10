# ========================================
# OpenCV Build Configuration
# ========================================
# This file configures OpenCV as a submodule with proper settings
# for OCR multi-language support (freetype for Chinese/Japanese/Korean text rendering)

option(BUILD_OPENCV_FROM_SOURCE "Build OpenCV from source (submodule) instead of using system OpenCV" ON)
option(OPENCV_ENABLE_NONFREE "Enable non-free algorithms in OpenCV" OFF)

if(NOT BUILD_OPENCV_FROM_SOURCE)
    message(STATUS "Using system-installed OpenCV")
    find_package(OpenCV REQUIRED)
    
    # Check if OpenCV has freetype support for multi-language rendering
    if(OpenCV_FOUND)
        message(STATUS "OpenCV found: ${OpenCV_VERSION}")
        message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
    endif()
else()
    message(STATUS "Building OpenCV from source (submodule)")
    
    # Check if opencv submodule exists
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/3rd-party/opencv/CMakeLists.txt")
        message(FATAL_ERROR "OpenCV submodule not found. Run: git submodule update --init 3rd-party/opencv 3rd-party/opencv_contrib")
    endif()
    
    # CRITICAL: Set all OpenCV options BEFORE add_subdirectory
    set(BUILD_opencv_world OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_python2 OFF CACHE BOOL "" FORCE)
    set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)
    
    # Enable only necessary modules for OCR
    set(BUILD_LIST "core,imgproc,imgcodecs,highgui" CACHE STRING "" FORCE)
    
    # Contrib modules configuration (for freetype support)
    if(EXISTS "${CMAKE_SOURCE_DIR}/3rd-party/opencv_contrib/modules")
        message(STATUS "OpenCV contrib modules found - enabling freetype")
        set(OPENCV_EXTRA_MODULES_PATH "${CMAKE_SOURCE_DIR}/3rd-party/opencv_contrib/modules" CACHE PATH "" FORCE)
        set(BUILD_opencv_freetype ON CACHE BOOL "" FORCE)
        set(BUILD_LIST "core,imgproc,imgcodecs,highgui,freetype" CACHE STRING "" FORCE)
    else()
        message(WARNING "OpenCV contrib not found. Multi-language text rendering (freetype) will not be available.")
    endif()
    
    # Minimal options for faster build
    set(WITH_GTK OFF CACHE BOOL "" FORCE)
    set(WITH_QT OFF CACHE BOOL "" FORCE)
    set(WITH_OPENGL OFF CACHE BOOL "" FORCE)
    set(WITH_FFMPEG OFF CACHE BOOL "" FORCE)
    set(WITH_V4L OFF CACHE BOOL "" FORCE)
    set(WITH_1394 OFF CACHE BOOL "" FORCE)
    set(WITH_GSTREAMER OFF CACHE BOOL "" FORCE)
    
    # Image format support
    set(WITH_JPEG ON CACHE BOOL "" FORCE)
    set(WITH_PNG ON CACHE BOOL "" FORCE)
    set(WITH_TIFF ON CACHE BOOL "" FORCE)
    set(WITH_WEBP ON CACHE BOOL "" FORCE)
    
    # Performance
    set(ENABLE_FAST_MATH ON CACHE BOOL "" FORCE)
    set(CV_ENABLE_INTRINSICS ON CACHE BOOL "" FORCE)
    set(WITH_IPP OFF CACHE BOOL "" FORCE)
    set(WITH_TBB OFF CACHE BOOL "" FORCE)
    set(WITH_EIGEN OFF CACHE BOOL "" FORCE)
    
    # Add opencv subdirectory - this will be built as a dependency
    add_subdirectory(${CMAKE_SOURCE_DIR}/3rd-party/opencv ${CMAKE_BINARY_DIR}/opencv_build EXCLUDE_FROM_ALL)
    
    # Set OpenCV_DIR for the rest of the project
    set(OpenCV_DIR "${CMAKE_BINARY_DIR}/opencv_build" CACHE PATH "OpenCV build dir" FORCE)
    
    # Set OpenCV include directories for project modules
    # Order matters: build dir first (generated headers like opencv_modules.hpp),
    # then opencv_build, then source modules
    set(OpenCV_INCLUDE_DIRS 
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_BINARY_DIR}/opencv_build"
        "${CMAKE_SOURCE_DIR}/3rd-party/opencv/include"
        "${CMAKE_SOURCE_DIR}/3rd-party/opencv/modules/core/include"
        "${CMAKE_SOURCE_DIR}/3rd-party/opencv/modules/imgproc/include"
        "${CMAKE_SOURCE_DIR}/3rd-party/opencv/modules/imgcodecs/include"
        "${CMAKE_SOURCE_DIR}/3rd-party/opencv/modules/highgui/include"
        "${CMAKE_SOURCE_DIR}/3rd-party/opencv_contrib/modules/freetype/include"
        CACHE INTERNAL "OpenCV include directories"
    )
    
    # Set OpenCV libraries for project modules
    set(OpenCV_LIBS opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_freetype CACHE INTERNAL "OpenCV libraries")
    
    message(STATUS "✓ OpenCV will be built from source")
    message(STATUS "  Build directory: ${CMAKE_BINARY_DIR}/opencv_build")
    message(STATUS "  Modules: ${BUILD_LIST}")
endif()

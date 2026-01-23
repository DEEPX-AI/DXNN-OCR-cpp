# ========================================
# Server Unit Tests CMakeLists.txt
# ========================================
cmake_minimum_required(VERSION 3.15)

# ========================================
# Dependencies
# ========================================
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)

# Poppler is already configured in server/CMakeLists.txt via FetchPoppler.cmake
# The 'poppler' and 'poppler-cpp' targets are available as GLOBAL imported targets

# ========================================
# Test Sources
# ========================================
set(TEST_SOURCES
    test_main.cpp
    test_ocr_request.cpp
    test_json_response.cpp
    test_file_handler.cpp
    test_pdf_handler.cpp
    test_ssl_verification.cpp
    test_cli_args.cpp
    test_request_size_limits.cpp
    stress_test.cpp
    # Server source files (modules to test)
    ${CMAKE_SOURCE_DIR}/server/ocr_handler.cpp
    ${CMAKE_SOURCE_DIR}/server/json_response.cpp
    ${CMAKE_SOURCE_DIR}/server/file_handler.cpp
    ${CMAKE_SOURCE_DIR}/server/pdf_handler.cpp
    ${CMAKE_SOURCE_DIR}/3rd-party/cpp-base64/base64.cpp
)

# Create test executable
add_executable(server_tests ${TEST_SOURCES})

# ========================================
# Include Directories
# ========================================
target_include_directories(server_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/3rd-party/crow/include
    ${CMAKE_SOURCE_DIR}/3rd-party/cpp-base64
    ${CMAKE_SOURCE_DIR}/3rd-party/json/include
    ${CMAKE_SOURCE_DIR}/3rd-party/spdlog/include
    ${OpenCV_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
)

# ========================================
# Link Libraries
# ========================================
set(TEST_LINK_LIBS
    gtest
    gtest_main
    gmock
    gmock_main
    ocr_pipeline
    ocr_detection
    ocr_classification
    ocr_recognition
    ocr_preprocessing
    ocr_common
    ${OpenCV_LIBS}
    CURL::libcurl
    ${UUID_LIBRARIES}
    pthread
)

# Add Poppler if target exists (created by FetchPoppler.cmake as GLOBAL IMPORTED)
if(TARGET poppler-cpp)
    # Get include directories from the imported target
    get_target_property(POPPLER_CPP_INCLUDES poppler-cpp INTERFACE_INCLUDE_DIRECTORIES)
    if(POPPLER_CPP_INCLUDES)
        target_include_directories(server_tests PRIVATE ${POPPLER_CPP_INCLUDES})
    endif()
    
    list(APPEND TEST_LINK_LIBS poppler-cpp poppler)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    list(APPEND TEST_LINK_LIBS ${FONTCONFIG_LIBRARIES} ${FREETYPE_LIBRARIES})
    message(STATUS "Server tests: Poppler enabled (includes: ${POPPLER_CPP_INCLUDES})")
else()
    message(WARNING "Server tests: Poppler NOT found (poppler-cpp target missing)")
endif()

target_link_libraries(server_tests PRIVATE ${TEST_LINK_LIBS})

# ========================================
# Target Properties
# ========================================
set_target_properties(server_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ========================================
# CTest Integration
# ========================================
add_test(NAME ServerUnitTests COMMAND server_tests)

# ========================================
# Status
# ========================================
message(STATUS "Server tests executable: server_tests")

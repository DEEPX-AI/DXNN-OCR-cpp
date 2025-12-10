cmake_minimum_required(VERSION 3.14)

# Test source files
set(TEST_REC_SOURCES
    test_recognizer.cpp
)

set(TEST_REC_MOBILE_SOURCES
    test_recognizer_mobile.cpp
)

# Create test executable for server models
add_executable(test_recognizer ${TEST_REC_SOURCES})

# Link libraries
target_link_libraries(test_recognizer
    PRIVATE
        ocr_recognition
        ocr_detection
        ocr_preprocessing
        ocr_common
        ${OpenCV_LIBS}
        dxrt
)

# Include directories
target_include_directories(test_recognizer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

# Set output directory to build root
set_target_properties(test_recognizer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Create test executable for mobile models
add_executable(test_recognizer_mobile ${TEST_REC_MOBILE_SOURCES})

# Link libraries
target_link_libraries(test_recognizer_mobile
    PRIVATE
        ocr_recognition
        ocr_detection
        ocr_preprocessing
        ocr_common
        ${OpenCV_LIBS}
        dxrt
)

# Include directories
target_include_directories(test_recognizer_mobile
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

# Set output directory to build root
set_target_properties(test_recognizer_mobile PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Recognition tests configured (server + mobile)")

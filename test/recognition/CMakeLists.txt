cmake_minimum_required(VERSION 3.14)

# Test source files
set(TEST_REC_SOURCES
    test_recognizer.cpp
)

# Create test executable
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

message(STATUS "Recognition test configured")

# Recognition module
cmake_minimum_required(VERSION 3.14)

# Source files
set(RECOGNITION_SOURCES
    text_recognizer.cpp
    rec_postprocess.cpp
)

# Create recognition library
add_library(ocr_recognition STATIC ${RECOGNITION_SOURCES})

# Ensure OpenCV is built before this target
if(TARGET opencv_core)
    add_dependencies(ocr_recognition opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_freetype)
endif()

target_include_directories(ocr_recognition PUBLIC ${OpenCV_INCLUDE_DIRS})

# Link dependencies
target_link_libraries(ocr_recognition
    PUBLIC
        ${OpenCV_LIBS}
    PRIVATE
        ocr_common
        ocr_preprocessing
)

# Include directories
target_include_directories(ocr_recognition
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

message(STATUS "Recognition module configured")

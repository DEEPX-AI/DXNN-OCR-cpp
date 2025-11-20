# Detection module test executable
add_executable(test_detector test_detector.cpp)

target_link_libraries(test_detector
    PRIVATE
        ocr_detection
        ocr_preprocessing
        ocr_common
        ${OpenCV_LIBS}
        dxrt
)

target_include_directories(test_detector 
    PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
)

# Set output directory to build root
set_target_properties(test_detector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

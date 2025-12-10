# Classification module
add_library(ocr_classification STATIC
    text_classifier.cpp
)

# Ensure OpenCV is built before this target
if(TARGET opencv_core)
    add_dependencies(ocr_classification opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_freetype)
endif()

target_include_directories(ocr_classification
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PUBLIC ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ocr_classification
    PRIVATE dxrt
    PRIVATE ${OpenCV_LIBS}
    PRIVATE ocr_common
    PRIVATE ocr_preprocessing
)

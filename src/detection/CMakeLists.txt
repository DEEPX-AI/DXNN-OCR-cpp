# Detection library

file(GLOB SOURCES "*.cpp")

add_library(ocr_detection STATIC ${SOURCES})

# Ensure OpenCV is built before this target
if(TARGET opencv_core)
    add_dependencies(ocr_detection opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_freetype)
endif()

target_include_directories(ocr_detection PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ocr_detection 
    PUBLIC 
        ocr_common
        ocr_preprocessing
        Clipper2
        ${OpenCV_LIBS}
        ${DXRT_LIBS}
)

# Export for other modules
set(OCR_DETECTION_LIBRARY ocr_detection PARENT_SCOPE)

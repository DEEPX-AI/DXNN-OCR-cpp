# Detection library

file(GLOB SOURCES "*.cpp")

add_library(ocr_detection STATIC ${SOURCES})

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

# Classification module
add_library(ocr_classification STATIC
    text_classifier.cpp
)

target_include_directories(ocr_classification
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ocr_classification
    PRIVATE dxrt
    PRIVATE ${OpenCV_LIBS}
    PRIVATE ocr_common
    PRIVATE ocr_preprocessing
)

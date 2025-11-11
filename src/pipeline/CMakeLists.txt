# Pipeline模块

add_library(ocr_pipeline
    ocr_pipeline.cpp
)

target_link_libraries(ocr_pipeline
    ocr_detection
    ocr_recognition
    ocr_common
    ocr_preprocessing
    ${OpenCV_LIBS}
)

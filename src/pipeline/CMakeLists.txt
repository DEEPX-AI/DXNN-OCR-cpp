# Pipeline模块

add_library(ocr_pipeline
    ocr_pipeline.cpp
    document_orientation.cpp
    document_preprocessing.cpp
)

target_link_libraries(ocr_pipeline
    ocr_detection
    ocr_classification
    ocr_recognition
    ocr_common
    ocr_preprocessing
    ${OpenCV_LIBS}
)

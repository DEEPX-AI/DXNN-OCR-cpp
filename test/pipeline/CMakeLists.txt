add_executable(test_pipeline
    test_pipeline.cpp
)

target_link_libraries(test_pipeline
    ocr_pipeline
    ocr_detection
    ocr_recognition
    ocr_common
    ocr_preprocessing
    ${OpenCV_LIBS}
    dxrt
    stdc++fs  # For std::filesystem
)

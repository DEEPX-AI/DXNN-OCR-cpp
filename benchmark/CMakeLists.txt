add_executable(benchmark
    benchmark.cpp
)

target_link_libraries(benchmark
    ocr_pipeline
    ocr_detection
    ocr_recognition
    ocr_common
    ocr_preprocessing
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
    dxrt
    stdc++fs
)

# Preprocessing library

file(GLOB SOURCES "*.cpp")

add_library(ocr_preprocessing STATIC ${SOURCES})

target_link_libraries(ocr_preprocessing PUBLIC ${OpenCV_LIBS})

# Export for other modules
set(OCR_PREPROCESSING_LIBRARY ocr_preprocessing PARENT_SCOPE)

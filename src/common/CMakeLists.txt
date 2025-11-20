# Common utilities library

file(GLOB SOURCES "*.cpp")

add_library(ocr_common STATIC ${SOURCES})

target_link_libraries(ocr_common PUBLIC ${OpenCV_LIBS})

# Export for other modules
set(OCR_COMMON_LIBRARY ocr_common PARENT_SCOPE)

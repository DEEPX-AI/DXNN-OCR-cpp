# Preprocessing library

file(GLOB SOURCES "*.cpp")

add_library(ocr_preprocessing STATIC ${SOURCES})

# Ensure OpenCV is built before this target
if(TARGET opencv_core)
    add_dependencies(ocr_preprocessing opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_freetype)
endif()

target_include_directories(ocr_preprocessing PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ocr_preprocessing PUBLIC ocr_common ${OpenCV_LIBS})

# Export for other modules
set(OCR_PREPROCESSING_LIBRARY ocr_preprocessing PARENT_SCOPE)

# Common utilities library

file(GLOB SOURCES "*.cpp")

add_library(ocr_common STATIC ${SOURCES})

# Ensure OpenCV is built before this target
if(TARGET opencv_core)
    add_dependencies(ocr_common opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_freetype)
endif()

target_include_directories(ocr_common PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ocr_common PUBLIC ${OpenCV_LIBS})

# Export for other modules
set(OCR_COMMON_LIBRARY ocr_common PARENT_SCOPE)

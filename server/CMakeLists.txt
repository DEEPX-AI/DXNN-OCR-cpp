# ========================================
# Server CMakeLists.txt
# ========================================
# HTTP API Server for DeepX OCR
cmake_minimum_required(VERSION 3.15)

# ========================================
# Dependencies
# ========================================
# CURL for HTTP client
find_package(CURL REQUIRED)

# UUID library (via pkg-config)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)

# ========================================
# Poppler Configuration (PDF rendering library)
# ========================================
# Poppler provides good thread safety for concurrent PDF rendering
# Source is downloaded and compiled automatically
message(STATUS "Using Poppler for PDF rendering")
include(${CMAKE_SOURCE_DIR}/cmake/FetchPoppler.cmake)

# ========================================
# Server Sources
# ========================================
set(SERVER_SOURCES
    server_main.cpp
    ocr_handler.cpp
    json_response.cpp
    file_handler.cpp
    pdf_handler.cpp
    ${CMAKE_SOURCE_DIR}/3rd-party/cpp-base64/base64.cpp
)

# Create server executable
add_executable(ocr_server ${SERVER_SOURCES})

# ========================================
# Include Directories
# ========================================
target_include_directories(ocr_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rd-party/crow/include
    ${CMAKE_SOURCE_DIR}/3rd-party/cpp-base64
    ${CMAKE_SOURCE_DIR}/3rd-party/json/include
    ${CMAKE_SOURCE_DIR}/3rd-party/spdlog/include
    ${OpenCV_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
)

# Add Poppler include directories
if(Poppler_FOUND)
    target_include_directories(ocr_server PRIVATE ${POPPLER_INCLUDE_DIRS})
endif()

# ========================================
# Link Libraries
# ========================================
set(SERVER_LINK_LIBS
    ocr_pipeline
    ocr_detection
    ocr_classification
    ocr_recognition
    ocr_preprocessing
    ocr_common
    ${OpenCV_LIBS}
    CURL::libcurl
    ${UUID_LIBRARIES}
    pthread
)

# Add Poppler libraries
if(Poppler_FOUND)
    list(APPEND SERVER_LINK_LIBS poppler-cpp poppler)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    list(APPEND SERVER_LINK_LIBS ${FONTCONFIG_LIBRARIES} ${FREETYPE_LIBRARIES})
endif()

target_link_libraries(ocr_server PRIVATE ${SERVER_LINK_LIBS})

# ========================================
# Target Properties
# ========================================
set_target_properties(ocr_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ========================================
# Install Rules
# ========================================
install(TARGETS ocr_server DESTINATION bin)

# ========================================
# Status Messages
# ========================================
message(STATUS "========================================")
message(STATUS "Server Configuration")
message(STATUS "========================================")
message(STATUS "  Executable: ocr_server")
message(STATUS "  CURL: ${CURL_LIBRARIES}")
message(STATUS "  UUID: ${UUID_LIBRARIES}")
if(Poppler_FOUND)
    message(STATUS "  PDF Library: Poppler ${Poppler_VERSION}")
else()
    message(STATUS "  PDF Library: NOT FOUND")
endif()
message(STATUS "========================================")

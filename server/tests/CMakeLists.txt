# Server Unit Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# 查找必需的依赖
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)

# 测试源文件
set(TEST_SOURCES
    test_main.cpp
    test_ocr_request.cpp
    test_json_response.cpp
    test_file_handler.cpp
    stress_test.cpp
    # Server源文件（需要测试的模块）
    ${CMAKE_SOURCE_DIR}/server/ocr_handler.cpp
    ${CMAKE_SOURCE_DIR}/server/json_response.cpp
    ${CMAKE_SOURCE_DIR}/server/file_handler.cpp
    ${CMAKE_SOURCE_DIR}/3rd-party/cpp-base64/base64.cpp
)

# 创建测试可执行文件
add_executable(server_tests ${TEST_SOURCES})

# 包含目录
target_include_directories(server_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/3rd-party/crow/include
    ${CMAKE_SOURCE_DIR}/3rd-party/cpp-base64
    ${CMAKE_SOURCE_DIR}/3rd-party/json/include
    ${CMAKE_SOURCE_DIR}/3rd-party/spdlog/include
    ${OpenCV_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
)

# 链接库 - gtest/gmock 由根 CMakeLists.txt 已添加
target_link_libraries(server_tests PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
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

# C++17标准
set_target_properties(server_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 添加测试发现（使用 CTest）
add_test(NAME ServerUnitTests COMMAND server_tests)

# 打印信息
message(STATUS "Server tests executable: server_tests")

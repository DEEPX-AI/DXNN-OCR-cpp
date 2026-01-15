# Server API Benchmark CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# 查找必需的依赖
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# API Benchmark 源文件
set(API_BENCHMARK_SOURCES
    api_benchmark.cpp
)

# 创建 API benchmark 可执行文件
add_executable(api_benchmark ${API_BENCHMARK_SOURCES})

# 包含目录
target_include_directories(api_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rd-party/json/include
    ${CMAKE_SOURCE_DIR}/3rd-party/spdlog/include
)

# 链接库
target_link_libraries(api_benchmark PRIVATE
    CURL::libcurl
    Threads::Threads
)

# C++17标准
set_target_properties(api_benchmark PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 打印信息
message(STATUS "API Benchmark executable: api_benchmark")

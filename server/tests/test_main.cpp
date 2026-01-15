/**
 * @file test_main.cpp
 * @brief Google Test 主入口文件
 * 
 * 这是 OCR Server 单元测试的主入口，使用 Google Test 框架
 */

#include <gtest/gtest.h>
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "DeepX OCR Server - Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 初始化 Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // 运行所有测试
    return RUN_ALL_TESTS();
}

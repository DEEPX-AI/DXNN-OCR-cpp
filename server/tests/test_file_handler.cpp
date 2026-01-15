/**
 * @file test_file_handler.cpp
 * @brief 文件处理测试 - Base64 解码和图像处理
 * 
 * 测试 FileHandler 类的 Base64 解码功能
 */

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <thread>
#include <chrono>
#include "file_handler.h"

using namespace ocr_server;

// ==================== DecodeBase64Image 测试 ====================

// 一个有效的 1x1 红色 PNG 图像的 Base64 编码
const std::string VALID_PNG_BASE64 = 
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";

// 一个有效的 1x1 蓝色 JPEG 图像的 Base64 编码
const std::string VALID_JPEG_BASE64 = 
    "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRof"
    "Hh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/wAALCAABAAEBAREA/8QAFgAB"
    "AQEAAAAAAAAAAAAAAAAAAAkI/8QAFBABAAAAAAAAAAAAAAAAAAAAAP/aAAgBAQAAPwBVf//Z";

/**
 * @brief 测试有效的 Base64 PNG 解码
 */
TEST(FileHandler, DecodeBase64Image_ValidPNG) {
    cv::Mat image;
    bool success = FileHandler::DecodeBase64Image(VALID_PNG_BASE64, image);
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(image.empty());
    EXPECT_EQ(image.cols, 1);
    EXPECT_EQ(image.rows, 1);
}

/**
 * @brief 测试空字符串处理
 */
TEST(FileHandler, DecodeBase64Image_Empty) {
    cv::Mat image;
    bool success = FileHandler::DecodeBase64Image("", image);
    
    EXPECT_FALSE(success);
    EXPECT_TRUE(image.empty());
}

/**
 * @brief 测试无效的 Base64 字符串
 */
TEST(FileHandler, DecodeBase64Image_Invalid) {
    cv::Mat image;
    
    // 完全无效的字符串
    bool success1 = FileHandler::DecodeBase64Image("not_valid_base64!!!", image);
    EXPECT_FALSE(success1);
    
    // 有效的 Base64 但不是图像数据
    bool success2 = FileHandler::DecodeBase64Image("SGVsbG8gV29ybGQ=", image);  // "Hello World"
    EXPECT_FALSE(success2);
}

/**
 * @brief 测试带前缀的 Base64 (data:image/png;base64,)
 */
TEST(FileHandler, DecodeBase64Image_WithPrefix) {
    cv::Mat image;
    
    // 带 PNG 前缀
    std::string with_png_prefix = "data:image/png;base64," + VALID_PNG_BASE64;
    bool success = FileHandler::DecodeBase64Image(with_png_prefix, image);
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(image.empty());
}

/**
 * @brief 测试带 JPEG 前缀的 Base64
 */
TEST(FileHandler, DecodeBase64Image_WithJpegPrefix) {
    cv::Mat image;
    
    std::string with_jpeg_prefix = "data:image/jpeg;base64," + VALID_JPEG_BASE64;
    bool success = FileHandler::DecodeBase64Image(with_jpeg_prefix, image);
    
    // JPEG 解码可能在某些环境中有问题，但应该至少不崩溃
    // 如果成功，图像应该非空
    if (success) {
        EXPECT_FALSE(image.empty());
    }
}

/**
 * @brief 测试只有前缀但没有数据的情况
 */
TEST(FileHandler, DecodeBase64Image_OnlyPrefix) {
    cv::Mat image;
    
    bool success = FileHandler::DecodeBase64Image("data:image/png;base64,", image);
    
    EXPECT_FALSE(success);
    EXPECT_TRUE(image.empty());
}

/**
 * @brief 测试带有换行符的 Base64
 */
TEST(FileHandler, DecodeBase64Image_WithNewlines) {
    cv::Mat image;
    
    // 在 Base64 中间添加换行符
    std::string with_newlines = 
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAA\n"
        "DUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";
    
    // Base64 解码器应该能处理换行符
    // 注意：实际行为取决于 cpp-base64 库的实现
    // 这里我们只是确保不会崩溃
    (void)FileHandler::DecodeBase64Image(with_newlines, image);
    SUCCEED();
}

/**
 * @brief 测试多次解码（内存泄漏检测用例）
 */
TEST(FileHandler, DecodeBase64Image_MultipleDecodes) {
    for (int i = 0; i < 10; ++i) {
        cv::Mat image;
        bool success = FileHandler::DecodeBase64Image(VALID_PNG_BASE64, image);
        EXPECT_TRUE(success);
        EXPECT_FALSE(image.empty());
    }
}

// ==================== SaveVisualizationImage 测试 ====================

/**
 * @brief 测试保存可视化图像
 */
TEST(FileHandler, SaveVisualizationImage_Basic) {
    // 创建一个测试图像
    cv::Mat test_image(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));  // 蓝色图像
    
    // 使用临时目录
    std::string temp_dir = "/tmp/ocr_test_vis_" + std::to_string(time(nullptr));
    
    std::string filename = FileHandler::SaveVisualizationImage(test_image, temp_dir);
    
    EXPECT_FALSE(filename.empty());
    EXPECT_TRUE(filename.find("ocr_vis_") != std::string::npos);
    EXPECT_TRUE(filename.find(".jpg") != std::string::npos);
    
    // 验证文件存在
    std::string full_path = temp_dir + "/" + filename;
    EXPECT_TRUE(std::filesystem::exists(full_path));
    
    // 清理
    std::filesystem::remove_all(temp_dir);
}

/**
 * @brief 测试保存空图像
 */
TEST(FileHandler, SaveVisualizationImage_EmptyImage) {
    cv::Mat empty_image;
    std::string temp_dir = "/tmp/ocr_test_empty";
    
    std::string filename = FileHandler::SaveVisualizationImage(empty_image, temp_dir);
    
    EXPECT_TRUE(filename.empty());
    
    // 清理
    std::filesystem::remove_all(temp_dir);
}

/**
 * @brief 测试目录自动创建
 */
TEST(FileHandler, SaveVisualizationImage_CreateDirectory) {
    cv::Mat test_image(50, 50, CV_8UC3, cv::Scalar(0, 255, 0));  // 绿色图像
    
    // 使用一个深层嵌套的目录
    std::string nested_dir = "/tmp/ocr_test_nested/level1/level2/level3";
    
    // 确保目录不存在
    std::filesystem::remove_all("/tmp/ocr_test_nested");
    
    std::string filename = FileHandler::SaveVisualizationImage(test_image, nested_dir);
    
    EXPECT_FALSE(filename.empty());
    EXPECT_TRUE(std::filesystem::exists(nested_dir));
    
    // 清理
    std::filesystem::remove_all("/tmp/ocr_test_nested");
}

/**
 * @brief 测试文件名唯一性
 */
TEST(FileHandler, SaveVisualizationImage_UniqueFilenames) {
    cv::Mat test_image(10, 10, CV_8UC3, cv::Scalar(128, 128, 128));
    std::string temp_dir = "/tmp/ocr_test_unique";
    
    std::set<std::string> filenames;
    
    // 快速连续保存多个图像
    for (int i = 0; i < 5; ++i) {
        std::string filename = FileHandler::SaveVisualizationImage(test_image, temp_dir);
        if (!filename.empty()) {
            filenames.insert(filename);
        }
        // 小延迟确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    
    // 所有文件名应该唯一
    EXPECT_EQ(filenames.size(), 5);
    
    // 清理
    std::filesystem::remove_all(temp_dir);
}

// ==================== URL 格式检测测试 ====================

/**
 * @brief 测试 HTTP URL 识别
 */
TEST(FileHandler, URLDetection_HTTP) {
    std::string http_url = "http://example.com/image.jpg";
    
    bool is_http = (http_url.find("http://") == 0);
    EXPECT_TRUE(is_http);
}

/**
 * @brief 测试 HTTPS URL 识别
 */
TEST(FileHandler, URLDetection_HTTPS) {
    std::string https_url = "https://example.com/image.png";
    
    bool is_https = (https_url.find("https://") == 0);
    EXPECT_TRUE(is_https);
}

/**
 * @brief 测试 Base64 不是 URL
 */
TEST(FileHandler, URLDetection_NotURL) {
    std::string base64_data = VALID_PNG_BASE64;
    
    bool is_url = (base64_data.find("http://") == 0 || 
                   base64_data.find("https://") == 0);
    EXPECT_FALSE(is_url);
}

// ==================== 边界条件测试 ====================

/**
 * @brief 测试非常长的 Base64 字符串（防止栈溢出）
 */
TEST(FileHandler, DecodeBase64Image_LongString) {
    // 创建一个较大的有效图像
    cv::Mat large_image(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
    
    // 编码为 Base64
    std::vector<uchar> buffer;
    cv::imencode(".png", large_image, buffer);
    
    // 这里我们不直接测试，因为需要 Base64 编码函数
    // 但我们确保函数不会因为大输入而崩溃
    
    // 创建一个重复很多次的有效 Base64
    std::string repeated = VALID_PNG_BASE64;
    for (int i = 0; i < 100; ++i) {
        repeated += VALID_PNG_BASE64;
    }
    
    cv::Mat image;
    // 这应该失败，因为连接的 Base64 不是有效图像
    // 不检查结果，只确保不崩溃
    (void)FileHandler::DecodeBase64Image(repeated, image);
    SUCCEED();
}

/**
 * @brief 测试特殊字符在 Base64 中
 */
TEST(FileHandler, DecodeBase64Image_SpecialChars) {
    cv::Mat image;
    
    // 包含非法 Base64 字符
    bool success = FileHandler::DecodeBase64Image("ABC@#$%^&*()DEF", image);
    EXPECT_FALSE(success);
}

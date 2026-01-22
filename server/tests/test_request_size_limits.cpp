#include <gtest/gtest.h>
#include "ocr_handler.h"

using namespace ocr_server;

class OCRRequestSizeLimitTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 1: 正常大小的 Base64 应该通过验证
TEST_F(OCRRequestSizeLimitTest, ValidBase64SizeAccepted) {
    OCRRequest req;
    // 创建 1MB 的伪 Base64 数据
    req.file = std::string(1024 * 1024, 'A');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
    EXPECT_TRUE(error_msg.empty());
}

// 测试 2: 超过 50MB 的 Base64 应该被拒绝
TEST_F(OCRRequestSizeLimitTest, OversizedBase64Rejected) {
    OCRRequest req;
    // 创建 51MB 的数据（超过限制）
    req.file = std::string(51 * 1024 * 1024, 'A');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_NE(error_msg.find("File too large"), std::string::npos);
}

// 测试 3: 边界值测试 - 恰好 50MB 应该通过
TEST_F(OCRRequestSizeLimitTest, ExactlyMaxBase64SizeAccepted) {
    OCRRequest req;
    req.file = std::string(50 * 1024 * 1024, 'A');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
}

// 测试 4: 边界值测试 - 50MB + 1 字节应该被拒绝
TEST_F(OCRRequestSizeLimitTest, OneBeyondMaxBase64SizeRejected) {
    OCRRequest req;
    req.file = std::string(50 * 1024 * 1024 + 1, 'A');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_FALSE(req.Validate(error_msg));
}

// 测试 5: 正常长度 URL 应该通过
TEST_F(OCRRequestSizeLimitTest, ValidURLLengthAccepted) {
    OCRRequest req;
    req.file = "https://example.com/image.jpg";
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
}

// 测试 6: 超过 2048 字符的 URL 应该被拒绝
TEST_F(OCRRequestSizeLimitTest, OversizedURLRejected) {
    OCRRequest req;
    // 创建超过 2048 字符的 URL
    req.file = "https://example.com/" + std::string(2100, 'a');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_NE(error_msg.find("URL too long"), std::string::npos);
}

// 测试 7: 边界值测试 - 恰好 2048 字符的 URL 应该通过
TEST_F(OCRRequestSizeLimitTest, ExactlyMaxURLLengthAccepted) {
    OCRRequest req;
    std::string base_url = "https://a.co/";
    req.file = base_url + std::string(2048 - base_url.length(), 'x');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
}

// 测试 8: HTTP URL 也应该被检查长度
TEST_F(OCRRequestSizeLimitTest, HttpURLAlsoChecked) {
    OCRRequest req;
    req.file = "http://example.com/" + std::string(2100, 'a');
    req.fileType = 1;
    
    std::string error_msg;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_NE(error_msg.find("URL too long"), std::string::npos);
}
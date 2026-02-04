/**
 * @file test_pdf_handler.cpp
 * @brief PDF 处理器测试 - Poppler 集成测试
 * 
 * 测试 PDFHandler 类的 PDF 渲染功能
 */

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>
#include "pdf_handler.h"
#include "base64.h"

using namespace ocr_server;

// ==================== PDFErrorCode 测试 ====================

/**
 * @brief 测试错误消息获取
 */
TEST(PDFHandler, GetErrorMessage_AllCodes) {
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::SUCCESS), "Success");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::CONFIG_ERROR), "Invalid PDF configuration parameters");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::FILE_ERROR), "PDF file cannot be opened");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::FORMAT_ERROR), "Invalid PDF format or corrupted file");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::PASSWORD_REQUIRED), "PDF is password protected");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::SECURITY_ERROR), "PDF security policy not supported");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::PAGE_ERROR), "PDF page not found");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::MEMORY_ERROR), "Memory allocation failed during PDF rendering");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::TIMEOUT_ERROR), "PDF page rendering timeout");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::PAGE_LIMIT_EXCEEDED), "PDF page count exceeds maximum limit");
    EXPECT_EQ(PDFHandler::GetErrorMessage(PDFErrorCode::DPI_LIMIT_EXCEEDED), "Requested DPI exceeds maximum limit");
    
    // 未知错误码
    EXPECT_EQ(PDFHandler::GetErrorMessage(9999), "Unknown PDF processing error");
}

/**
 * @brief 测试 HTTP 状态码映射
 */
TEST(PDFHandler, GetHttpStatusCode) {
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::SUCCESS), 200);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::CONFIG_ERROR), 400);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::FILE_ERROR), 400);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::FORMAT_ERROR), 400);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::PASSWORD_REQUIRED), 401);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::SECURITY_ERROR), 403);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::PAGE_ERROR), 400);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::MEMORY_ERROR), 503);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::TIMEOUT_ERROR), 504);
    EXPECT_EQ(PDFHandler::GetHttpStatusCode(PDFErrorCode::UNKNOWN_ERROR), 500);
}

/**
 * @brief 测试后端名称
 */
TEST(PDFHandler, GetBackendName) {
    EXPECT_EQ(PDFHandler::GetBackendName(), "Poppler");
}

// ==================== PDFRenderConfig 测试 ====================

/**
 * @brief 测试默认配置
 */
TEST(PDFRenderConfig, DefaultValues) {
    PDFRenderConfig config;
    
    EXPECT_EQ(config.dpi, 150);
    EXPECT_EQ(config.maxPages, 10);
    EXPECT_EQ(config.maxDpi, 300);
    EXPECT_EQ(config.maxPixelsPerPage, 25000000);
    EXPECT_EQ(config.renderTimeoutMs, 30000);
    EXPECT_EQ(config.maxConcurrentRenders, 4);
    EXPECT_FALSE(config.useAlpha);
}

// ==================== PDFHandler 构造和析构测试 ====================

/**
 * @brief 测试 PDFHandler 构造和析构
 */
TEST(PDFHandler, ConstructorDestructor) {
    // 确保构造和析构不崩溃
    {
        PDFHandler handler;
    }
    SUCCEED();
}

/**
 * @brief 测试多次构造
 */
TEST(PDFHandler, MultipleInstances) {
    // Poppler 不需要全局初始化，可以创建多个实例
    PDFHandler handler1;
    PDFHandler handler2;
    PDFHandler handler3;
    
    SUCCEED();
}

// ==================== RenderFromMemory 测试 ====================

/**
 * @brief 测试空数据
 */
TEST(PDFHandler, RenderFromMemory_EmptyData) {
    PDFHandler handler;
    std::vector<uint8_t> empty_data;
    
    PDFRenderResult result = handler.RenderFromMemory(empty_data);
    
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.errorCode, PDFErrorCode::SUCCESS);
}

/**
 * @brief 测试无效数据（非 PDF）
 */
TEST(PDFHandler, RenderFromMemory_InvalidData) {
    PDFHandler handler;
    
    // 随机数据
    std::vector<uint8_t> invalid_data = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    
    PDFRenderResult result = handler.RenderFromMemory(invalid_data);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, PDFErrorCode::FORMAT_ERROR);
}

/**
 * @brief 测试 DPI 超限
 */
TEST(PDFHandler, RenderFromMemory_ConfigError) {
    PDFHandler handler;
    std::vector<uint8_t> dummy_data = {'%', 'P', 'D', 'F'};  // PDF 魔术字节
    
    PDFRenderConfig config;
    config.dpi = 400;  // 超过默认 maxDpi=300
    
    PDFRenderResult result = handler.RenderFromMemory(dummy_data, config);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, PDFErrorCode::CONFIG_ERROR);
}

// ==================== RenderFromBase64 测试 ====================

/**
 * @brief 测试空 Base64
 */
TEST(PDFHandler, RenderFromBase64_Empty) {
    PDFHandler handler;
    
    PDFRenderResult result = handler.RenderFromBase64("");
    
    EXPECT_FALSE(result.success);
}

/**
 * @brief 测试无效 Base64
 */
TEST(PDFHandler, RenderFromBase64_InvalidBase64) {
    PDFHandler handler;
    
    // 无效的 Base64 字符串
    PDFRenderResult result = handler.RenderFromBase64("not_valid_base64!!!");
    
    EXPECT_FALSE(result.success);
}

/**
 * @brief 测试有效 Base64 但非 PDF 内容
 */
TEST(PDFHandler, RenderFromBase64_NotPDF) {
    PDFHandler handler;
    
    // "Hello World" 的 Base64 编码
    PDFRenderResult result = handler.RenderFromBase64("SGVsbG8gV29ybGQ=");
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, PDFErrorCode::FORMAT_ERROR);
}

// ==================== GetPageCount 测试 ====================

/**
 * @brief 测试空数据获取页数
 */
TEST(PDFHandler, GetPageCount_EmptyData) {
    PDFHandler handler;
    std::vector<uint8_t> empty_data;
    int errorCode;
    
    int pageCount = handler.GetPageCount(empty_data, errorCode);
    
    EXPECT_EQ(pageCount, -1);
    EXPECT_NE(errorCode, PDFErrorCode::SUCCESS);
}

/**
 * @brief 测试无效数据获取页数
 */
TEST(PDFHandler, GetPageCount_InvalidData) {
    PDFHandler handler;
    std::vector<uint8_t> invalid_data = {'N', 'O', 'T', ' ', 'P', 'D', 'F'};
    int errorCode;
    
    int pageCount = handler.GetPageCount(invalid_data, errorCode);
    
    EXPECT_EQ(pageCount, -1);
    EXPECT_EQ(errorCode, PDFErrorCode::FORMAT_ERROR);
}

// ==================== PDFRenderResult 测试 ====================

/**
 * @brief 测试 PDFRenderResult 默认值
 */
TEST(PDFRenderResult, DefaultValues) {
    PDFRenderResult result;
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, 0);
    EXPECT_TRUE(result.errorMsg.empty());
    EXPECT_EQ(result.totalPages, 0);
    EXPECT_EQ(result.renderedPages, 0);
    EXPECT_EQ(result.failedPages, 0);
    EXPECT_TRUE(result.pages.empty());
}

// ==================== PDFPageImage 测试 ====================

/**
 * @brief 测试 PDFPageImage 默认值
 */
TEST(PDFPageImage, DefaultValues) {
    PDFPageImage page;
    
    EXPECT_EQ(page.pageIndex, -1);  // 默认 -1 表示未初始化
    EXPECT_TRUE(page.image.empty());
    EXPECT_EQ(page.originalWidthPts, 0);
    EXPECT_EQ(page.originalHeightPts, 0);
    EXPECT_EQ(page.renderedWidth, 0);
    EXPECT_EQ(page.renderedHeight, 0);
    EXPECT_FALSE(page.success);
    EXPECT_EQ(page.errorCode, 0);
    EXPECT_TRUE(page.errorMsg.empty());
}

// ==================== 集成测试 (需要真实 PDF 文件) ====================

/**
 * @brief 测试类用于加载真实 PDF 文件
 */
class PDFHandlerIntegrationTest : public ::testing::Test {
protected:
    PDFHandler handler;
    
    // 查找测试 PDF 文件
    std::string FindTestPDF() {
        // 尝试几个可能的路径
        std::vector<std::string> possible_paths = {
            "3rd-party/opencv_contrib/modules/dnns_easily_fooled/Installation_Guide.pdf",
            "../3rd-party/opencv_contrib/modules/dnns_easily_fooled/Installation_Guide.pdf",
            "../../3rd-party/opencv_contrib/modules/dnns_easily_fooled/Installation_Guide.pdf"
        };
        
        for (const auto& path : possible_paths) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
        return "";
    }
    
    // 读取文件到 vector
    std::vector<uint8_t> ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }
};

/**
 * @brief 测试真实 PDF 渲染（如果测试 PDF 存在）
 */
TEST_F(PDFHandlerIntegrationTest, RenderRealPDF) {
    std::string pdf_path = FindTestPDF();
    if (pdf_path.empty()) {
        GTEST_SKIP() << "Test PDF file not found, skipping integration test";
    }
    
    std::vector<uint8_t> pdf_data = ReadFile(pdf_path);
    ASSERT_FALSE(pdf_data.empty()) << "Failed to read test PDF file";
    
    PDFRenderConfig config;
    config.dpi = 72;  // 低 DPI 以加快测试
    config.maxPages = 1;  // 只渲染第一页
    
    PDFRenderResult result = handler.RenderFromMemory(pdf_data, config);
    
    EXPECT_TRUE(result.success) << "Error: " << result.errorMsg;
    EXPECT_GT(result.totalPages, 0);
    EXPECT_EQ(result.renderedPages, 1);
    EXPECT_EQ(result.pages.size(), 1);
    
    if (!result.pages.empty()) {
        const auto& page = result.pages[0];
        EXPECT_TRUE(page.success);
        EXPECT_FALSE(page.image.empty());
        EXPECT_GT(page.renderedWidth, 0);
        EXPECT_GT(page.renderedHeight, 0);
        EXPECT_EQ(page.image.channels(), 3);  // BGR
    }
}

/**
 * @brief 测试多页 PDF 渲染
 */
TEST_F(PDFHandlerIntegrationTest, RenderMultiplePages) {
    std::string pdf_path = FindTestPDF();
    if (pdf_path.empty()) {
        GTEST_SKIP() << "Test PDF file not found, skipping integration test";
    }
    
    std::vector<uint8_t> pdf_data = ReadFile(pdf_path);
    ASSERT_FALSE(pdf_data.empty());
    
    PDFRenderConfig config;
    config.dpi = 72;
    config.maxPages = 3;  // 最多 3 页
    
    PDFRenderResult result = handler.RenderFromMemory(pdf_data, config);
    
    EXPECT_TRUE(result.success) << "Error: " << result.errorMsg;
    EXPECT_LE(result.renderedPages, 3);
    
    // 验证每页
    for (const auto& page : result.pages) {
        EXPECT_TRUE(page.success) << "Page " << page.pageIndex << " failed: " << page.errorMsg;
        EXPECT_FALSE(page.image.empty());
    }
}

/**
 * @brief 测试页数限制
 */
TEST_F(PDFHandlerIntegrationTest, PageLimit) {
    std::string pdf_path = FindTestPDF();
    if (pdf_path.empty()) {
        GTEST_SKIP() << "Test PDF file not found";
    }
    
    std::vector<uint8_t> pdf_data = ReadFile(pdf_path);
    ASSERT_FALSE(pdf_data.empty());
    
    PDFRenderConfig config;
    config.dpi = 72;
    config.maxPages = 2;
    
    PDFRenderResult result = handler.RenderFromMemory(pdf_data, config);
    
    // 如果 PDF 超过 2 页，应该只渲染 2 页
    if (result.totalPages > 2) {
        EXPECT_EQ(result.renderedPages, 2);
    }
}

/**
 * @brief 测试 DPI 对输出尺寸的影响
 */
TEST_F(PDFHandlerIntegrationTest, DPIEffect) {
    std::string pdf_path = FindTestPDF();
    if (pdf_path.empty()) {
        GTEST_SKIP() << "Test PDF file not found";
    }
    
    std::vector<uint8_t> pdf_data = ReadFile(pdf_path);
    ASSERT_FALSE(pdf_data.empty());
    
    // 低 DPI
    PDFRenderConfig config_low;
    config_low.dpi = 72;
    config_low.maxPages = 1;
    
    // 高 DPI
    PDFRenderConfig config_high;
    config_high.dpi = 150;
    config_high.maxPages = 1;
    
    PDFRenderResult result_low = handler.RenderFromMemory(pdf_data, config_low);
    PDFRenderResult result_high = handler.RenderFromMemory(pdf_data, config_high);
    
    ASSERT_TRUE(result_low.success);
    ASSERT_TRUE(result_high.success);
    ASSERT_FALSE(result_low.pages.empty());
    ASSERT_FALSE(result_high.pages.empty());
    
    // 高 DPI 的图像应该更大
    int low_pixels = result_low.pages[0].renderedWidth * result_low.pages[0].renderedHeight;
    int high_pixels = result_high.pages[0].renderedWidth * result_high.pages[0].renderedHeight;
    
    EXPECT_GT(high_pixels, low_pixels);
}

/**
 * @brief 测试 Base64 编码的 PDF
 */
TEST_F(PDFHandlerIntegrationTest, RenderFromBase64) {
    std::string pdf_path = FindTestPDF();
    if (pdf_path.empty()) {
        GTEST_SKIP() << "Test PDF file not found";
    }
    
    std::vector<uint8_t> pdf_data = ReadFile(pdf_path);
    ASSERT_FALSE(pdf_data.empty());
    
    // 编码为 Base64
    std::string base64_str = base64_encode(pdf_data.data(), pdf_data.size());
    ASSERT_FALSE(base64_str.empty());
    
    PDFRenderConfig config;
    config.dpi = 72;
    config.maxPages = 1;
    
    PDFRenderResult result = handler.RenderFromBase64(base64_str, config);
    
    EXPECT_TRUE(result.success) << "Error: " << result.errorMsg;
    EXPECT_EQ(result.renderedPages, 1);
}

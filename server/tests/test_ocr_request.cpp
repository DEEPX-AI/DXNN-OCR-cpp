/**
 * @file test_ocr_request.cpp
 * @brief OCRRequest 参数解析和验证测试
 * 
 * 参考 OCR-py tests/pipelines/test_ocr.py 的测试模式
 * 覆盖所有 API 参数的解析和验证逻辑
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "ocr_handler.h"

using json = nlohmann::json;
using namespace ocr_server;

// ==================== OCRRequest::FromJson 测试 ====================

/**
 * @brief 测试默认值：验证所有参数的默认值是否正确
 */
TEST(OCRRequestFromJson, DefaultValues) {
    json j;
    j["file"] = "test_base64_data";
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    // 验证所有默认值
    EXPECT_EQ(req.file, "test_base64_data");
    EXPECT_EQ(req.fileType, 1);  // 默认为图像
    EXPECT_FALSE(req.useDocOrientationClassify);
    EXPECT_FALSE(req.useDocUnwarping);
    EXPECT_FALSE(req.useTextlineOrientation);
    EXPECT_EQ(req.textDetLimitSideLen, 64);
    EXPECT_EQ(req.textDetLimitType, "min");
    EXPECT_DOUBLE_EQ(req.textDetThresh, 0.3);
    EXPECT_DOUBLE_EQ(req.textDetBoxThresh, 0.6);
    EXPECT_DOUBLE_EQ(req.textDetUnclipRatio, 1.5);
    EXPECT_DOUBLE_EQ(req.textRecScoreThresh, 0.0);
    EXPECT_FALSE(req.visualize);
}

/**
 * @brief 测试所有参数解析：验证所有参数都能正确解析
 */
TEST(OCRRequestFromJson, AllParametersParsed) {
    json j;
    j["file"] = "base64_image_data_here";
    j["fileType"] = 1;
    j["useDocOrientationClassify"] = true;
    j["useDocUnwarping"] = true;
    j["useTextlineOrientation"] = true;
    j["textDetLimitSideLen"] = 960;
    j["textDetLimitType"] = "max";
    j["textDetThresh"] = 0.5;
    j["textDetBoxThresh"] = 0.7;
    j["textDetUnclipRatio"] = 2.0;
    j["textRecScoreThresh"] = 0.3;
    j["visualize"] = true;
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    EXPECT_EQ(req.file, "base64_image_data_here");
    EXPECT_EQ(req.fileType, 1);
    EXPECT_TRUE(req.useDocOrientationClassify);
    EXPECT_TRUE(req.useDocUnwarping);
    EXPECT_TRUE(req.useTextlineOrientation);
    EXPECT_EQ(req.textDetLimitSideLen, 960);
    EXPECT_EQ(req.textDetLimitType, "max");
    EXPECT_DOUBLE_EQ(req.textDetThresh, 0.5);
    EXPECT_DOUBLE_EQ(req.textDetBoxThresh, 0.7);
    EXPECT_DOUBLE_EQ(req.textDetUnclipRatio, 2.0);
    EXPECT_DOUBLE_EQ(req.textRecScoreThresh, 0.3);
    EXPECT_TRUE(req.visualize);
}

/**
 * @brief 测试部分参数：只提供部分参数，其他使用默认值
 */
TEST(OCRRequestFromJson, PartialParameters) {
    json j;
    j["file"] = "partial_test";
    j["useDocOrientationClassify"] = true;
    j["textDetThresh"] = 0.4;
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    // 显式设置的参数
    EXPECT_EQ(req.file, "partial_test");
    EXPECT_TRUE(req.useDocOrientationClassify);
    EXPECT_DOUBLE_EQ(req.textDetThresh, 0.4);
    
    // 默认值
    EXPECT_EQ(req.fileType, 1);
    EXPECT_FALSE(req.useDocUnwarping);
    EXPECT_FALSE(req.useTextlineOrientation);
    EXPECT_EQ(req.textDetLimitSideLen, 64);
    EXPECT_DOUBLE_EQ(req.textDetBoxThresh, 0.6);
}

/**
 * @brief 测试空 JSON：没有提供任何参数
 */
TEST(OCRRequestFromJson, EmptyJson) {
    json j = json::object();
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    EXPECT_TRUE(req.file.empty());
    EXPECT_EQ(req.fileType, 1);
}

/**
 * @brief 测试 URL 格式的 file 参数
 */
TEST(OCRRequestFromJson, URLFileParameter) {
    json j;
    j["file"] = "https://example.com/image.jpg";
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    EXPECT_EQ(req.file, "https://example.com/image.jpg");
}

/**
 * @brief 参考 OCR-py test_predict_params 的参数组合测试
 */
TEST(OCRRequestFromJson, UseDocOrientationClassifyParam) {
    json j;
    j["file"] = "test";
    j["useDocOrientationClassify"] = false;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_FALSE(req.useDocOrientationClassify);
}

TEST(OCRRequestFromJson, UseDocUnwarpingParam) {
    json j;
    j["file"] = "test";
    j["useDocUnwarping"] = false;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_FALSE(req.useDocUnwarping);
}

TEST(OCRRequestFromJson, UseTextlineOrientationParam) {
    json j;
    j["file"] = "test";
    j["useTextlineOrientation"] = false;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_FALSE(req.useTextlineOrientation);
}

TEST(OCRRequestFromJson, TextDetLimitParams) {
    json j;
    j["file"] = "test";
    j["textDetLimitSideLen"] = 640;
    j["textDetLimitType"] = "min";
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_EQ(req.textDetLimitSideLen, 640);
    EXPECT_EQ(req.textDetLimitType, "min");
}

TEST(OCRRequestFromJson, TextDetThreshParam) {
    json j;
    j["file"] = "test";
    j["textDetThresh"] = 0.5;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_DOUBLE_EQ(req.textDetThresh, 0.5);
}

TEST(OCRRequestFromJson, TextDetBoxThreshParam) {
    json j;
    j["file"] = "test";
    j["textDetBoxThresh"] = 0.3;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_DOUBLE_EQ(req.textDetBoxThresh, 0.3);
}

TEST(OCRRequestFromJson, TextDetUnclipRatioParam) {
    json j;
    j["file"] = "test";
    j["textDetUnclipRatio"] = 3.0;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_DOUBLE_EQ(req.textDetUnclipRatio, 3.0);
}

TEST(OCRRequestFromJson, TextRecScoreThreshParam) {
    json j;
    j["file"] = "test";
    j["textRecScoreThresh"] = 0.5;
    
    OCRRequest req = OCRRequest::FromJson(j);
    EXPECT_DOUBLE_EQ(req.textRecScoreThresh, 0.5);
}

/**
 * @brief 测试 PDF 参数解析
 */
TEST(OCRRequestFromJson, PDFParameters) {
    json j;
    j["file"] = "test_pdf_base64";
    j["fileType"] = 0;
    j["pdfDpi"] = 200;
    j["pdfMaxPages"] = 5;
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    EXPECT_EQ(req.fileType, 0);
    EXPECT_EQ(req.pdfDpi, 200);
    EXPECT_EQ(req.pdfMaxPages, 5);
}

/**
 * @brief 测试 PDF 参数默认值
 */
TEST(OCRRequestFromJson, PDFParametersDefault) {
    json j;
    j["file"] = "test";
    j["fileType"] = 0;
    
    OCRRequest req = OCRRequest::FromJson(j);
    
    EXPECT_EQ(req.pdfDpi, 150);       // 默认 150
    EXPECT_EQ(req.pdfMaxPages, 10);   // 默认 10
}

// ==================== OCRRequest::Validate 测试 ====================

/**
 * @brief 测试空 file 参数验证
 */
TEST(OCRRequestValidate, EmptyFile) {
    OCRRequest req;
    req.file = "";
    
    std::string error_msg;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "Missing required parameter: 'file'");
}

/**
 * @brief 测试 PDF fileType 现已支持
 */
TEST(OCRRequestValidate, PDFFileTypeSupported) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 0;  // PDF - 已实现
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));  // PDF 现在是有效的
    EXPECT_TRUE(error_msg.empty());
}

/**
 * @brief 测试无效的 fileType 值
 */
TEST(OCRRequestValidate, InvalidFileType) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 2;  // 无效值
    
    std::string error_msg;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "fileType must be 0 (PDF) or 1 (Image)");
}

/**
 * @brief 测试 PDF DPI 参数验证
 */
TEST(OCRRequestValidate, PDFDpiRange) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 0;  // PDF
    
    std::string error_msg;
    
    // 有效范围
    req.pdfDpi = 72;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.pdfDpi = 150;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.pdfDpi = 300;
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 超出范围
    req.pdfDpi = 50;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "pdfDpi must be in range [72, 300]");
    
    req.pdfDpi = 400;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "pdfDpi must be in range [72, 300]");
}

/**
 * @brief 测试 PDF 最大页数参数验证
 */
TEST(OCRRequestValidate, PDFMaxPagesRange) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 0;  // PDF
    req.pdfDpi = 150;
    
    std::string error_msg;
    
    // 有效范围
    req.pdfMaxPages = 1;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.pdfMaxPages = 50;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.pdfMaxPages = 100;
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 超出范围
    req.pdfMaxPages = 0;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "pdfMaxPages must be in range [1, 100]");
    
    req.pdfMaxPages = 101;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "pdfMaxPages must be in range [1, 100]");
}

/**
 * @brief 测试 textDetThresh 边界值
 */
TEST(OCRRequestValidate, TextDetThreshRange) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 1;
    
    std::string error_msg;
    
    // 正常范围内
    req.textDetThresh = 0.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textDetThresh = 1.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textDetThresh = 0.5;
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 超出范围
    req.textDetThresh = -0.1;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textDetThresh must be in range [0.0, 1.0]");
    
    req.textDetThresh = 1.1;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textDetThresh must be in range [0.0, 1.0]");
}

/**
 * @brief 测试 textDetBoxThresh 边界值
 */
TEST(OCRRequestValidate, TextDetBoxThreshRange) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 1;
    req.textDetThresh = 0.3;  // 先设置有效值
    
    std::string error_msg;
    
    // 正常范围内
    req.textDetBoxThresh = 0.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textDetBoxThresh = 1.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 超出范围
    req.textDetBoxThresh = -0.1;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textDetBoxThresh must be in range [0.0, 1.0]");
    
    req.textDetBoxThresh = 1.5;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textDetBoxThresh must be in range [0.0, 1.0]");
}

/**
 * @brief 测试 textDetUnclipRatio 边界值
 */
TEST(OCRRequestValidate, TextDetUnclipRatioRange) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 1;
    req.textDetThresh = 0.3;
    req.textDetBoxThresh = 0.6;
    
    std::string error_msg;
    
    // 正常范围内
    req.textDetUnclipRatio = 1.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textDetUnclipRatio = 3.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textDetUnclipRatio = 2.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 超出范围
    req.textDetUnclipRatio = 0.5;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textDetUnclipRatio must be in range [1.0, 3.0]");
    
    req.textDetUnclipRatio = 4.0;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textDetUnclipRatio must be in range [1.0, 3.0]");
}

/**
 * @brief 测试 textRecScoreThresh 边界值
 */
TEST(OCRRequestValidate, TextRecScoreThreshRange) {
    OCRRequest req;
    req.file = "test_data";
    req.fileType = 1;
    req.textDetThresh = 0.3;
    req.textDetBoxThresh = 0.6;
    req.textDetUnclipRatio = 1.5;
    
    std::string error_msg;
    
    // 正常范围内
    req.textRecScoreThresh = 0.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textRecScoreThresh = 1.0;
    EXPECT_TRUE(req.Validate(error_msg));
    
    req.textRecScoreThresh = 0.5;
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 超出范围
    req.textRecScoreThresh = -0.1;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textRecScoreThresh must be in range [0.0, 1.0]");
    
    req.textRecScoreThresh = 1.5;
    EXPECT_FALSE(req.Validate(error_msg));
    EXPECT_EQ(error_msg, "textRecScoreThresh must be in range [0.0, 1.0]");
}

/**
 * @brief 测试有效请求的验证
 */
TEST(OCRRequestValidate, ValidRequest) {
    OCRRequest req;
    req.file = "valid_base64_data";
    req.fileType = 1;
    req.useDocOrientationClassify = true;
    req.useDocUnwarping = true;
    req.useTextlineOrientation = true;
    req.textDetLimitSideLen = 960;
    req.textDetLimitType = "max";
    req.textDetThresh = 0.3;
    req.textDetBoxThresh = 0.6;
    req.textDetUnclipRatio = 1.5;
    req.textRecScoreThresh = 0.0;
    req.visualize = true;
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
    EXPECT_TRUE(error_msg.empty());
}

/**
 * @brief 测试最小有效请求
 */
TEST(OCRRequestValidate, MinimalValidRequest) {
    OCRRequest req;
    req.file = "minimal_test";  // 只需要必填字段
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
}

/**
 * @brief 测试 Base64 和 URL 格式的 file
 */
TEST(OCRRequestValidate, ValidFileFormats) {
    std::string error_msg;
    
    // Base64 格式
    OCRRequest req1;
    req1.file = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";
    EXPECT_TRUE(req1.Validate(error_msg));
    
    // HTTP URL 格式
    OCRRequest req2;
    req2.file = "http://example.com/image.png";
    EXPECT_TRUE(req2.Validate(error_msg));
    
    // HTTPS URL 格式
    OCRRequest req3;
    req3.file = "https://example.com/image.jpg";
    EXPECT_TRUE(req3.Validate(error_msg));
}

// ==================== 边界条件测试 ====================

/**
 * @brief 测试边界值组合
 */
TEST(OCRRequestValidate, BoundaryValuesCombination) {
    OCRRequest req;
    req.file = "test";
    req.textDetThresh = 0.0;     // 最小边界
    req.textDetBoxThresh = 1.0;  // 最大边界
    req.textDetUnclipRatio = 1.0;  // 最小边界
    req.textRecScoreThresh = 0.0;  // 最小边界
    
    std::string error_msg;
    EXPECT_TRUE(req.Validate(error_msg));
}

/**
 * @brief 测试 textDetLimitType 的不同值
 */
TEST(OCRRequestValidate, TextDetLimitTypeValues) {
    OCRRequest req;
    req.file = "test";
    
    std::string error_msg;
    
    // "min" - 有效
    req.textDetLimitType = "min";
    EXPECT_TRUE(req.Validate(error_msg));
    
    // "max" - 有效
    req.textDetLimitType = "max";
    EXPECT_TRUE(req.Validate(error_msg));
    
    // 无效值 - 根据代码实现，这只会产生警告，不会导致验证失败
    req.textDetLimitType = "invalid";
    EXPECT_TRUE(req.Validate(error_msg));  // 不会失败，只会警告
}

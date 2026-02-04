/**
 * @file test_json_response.cpp
 * @brief JSON 响应构建测试
 * 
 * 测试 JsonResponseBuilder 类的所有功能
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <regex>
#include "json_response.h"

using json = nlohmann::json;
using namespace ocr_server;

// ==================== GenerateUUID 测试 ====================

/**
 * @brief 测试 UUID 格式是否正确
 * UUID 格式: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 */
TEST(JsonResponseBuilder, GenerateUUID_Format) {
    std::string uuid = JsonResponseBuilder::GenerateUUID();
    
    // UUID 应该是 36 个字符 (32 个十六进制字符 + 4 个连字符)
    EXPECT_EQ(uuid.length(), 36);
    
    // 验证 UUID 格式
    std::regex uuid_regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    EXPECT_TRUE(std::regex_match(uuid, uuid_regex)) << "UUID format invalid: " << uuid;
}

/**
 * @brief 测试 UUID 唯一性
 */
TEST(JsonResponseBuilder, GenerateUUID_Uniqueness) {
    const int num_uuids = 100;
    std::set<std::string> uuids;
    
    for (int i = 0; i < num_uuids; ++i) {
        std::string uuid = JsonResponseBuilder::GenerateUUID();
        uuids.insert(uuid);
    }
    
    // 所有生成的 UUID 应该都是唯一的
    EXPECT_EQ(uuids.size(), num_uuids);
}

/**
 * @brief 测试 UUID 非空
 */
TEST(JsonResponseBuilder, GenerateUUID_NotEmpty) {
    std::string uuid = JsonResponseBuilder::GenerateUUID();
    EXPECT_FALSE(uuid.empty());
}

// ==================== BuildSuccessResponse 测试 ====================

/**
 * @brief 测试成功响应的基本结构
 */
TEST(JsonResponseBuilder, BuildSuccessResponse_Structure) {
    std::vector<ocr::PipelineOCRResult> results;
    
    json response = JsonResponseBuilder::BuildSuccessResponse(results, "");
    
    // 验证必需字段存在
    EXPECT_TRUE(response.contains("logId"));
    EXPECT_TRUE(response.contains("errorCode"));
    EXPECT_TRUE(response.contains("errorMsg"));
    EXPECT_TRUE(response.contains("result"));
    EXPECT_TRUE(response["result"].contains("ocrResults"));
}

/**
 * @brief 测试成功响应的错误码和消息
 */
TEST(JsonResponseBuilder, BuildSuccessResponse_ErrorCode) {
    std::vector<ocr::PipelineOCRResult> results;
    
    json response = JsonResponseBuilder::BuildSuccessResponse(results, "");
    
    EXPECT_EQ(response["errorCode"].get<int>(), ErrorCode::SUCCESS);
    EXPECT_EQ(response["errorMsg"].get<std::string>(), "Success");
}

/**
 * @brief 测试成功响应包含 OCR 结果
 */
TEST(JsonResponseBuilder, BuildSuccessResponse_WithResults) {
    std::vector<ocr::PipelineOCRResult> results;
    
    // 创建测试结果
    ocr::PipelineOCRResult result1;
    result1.text = "Hello World";
    result1.confidence = 0.95f;
    result1.box = {
        cv::Point2f(10.0f, 20.0f),
        cv::Point2f(100.0f, 20.0f),
        cv::Point2f(100.0f, 50.0f),
        cv::Point2f(10.0f, 50.0f)
    };
    results.push_back(result1);
    
    ocr::PipelineOCRResult result2;
    result2.text = "Test Text";
    result2.confidence = 0.88f;
    result2.box = {
        cv::Point2f(10.0f, 60.0f),
        cv::Point2f(80.0f, 60.0f),
        cv::Point2f(80.0f, 90.0f),
        cv::Point2f(10.0f, 90.0f)
    };
    results.push_back(result2);
    
    json response = JsonResponseBuilder::BuildSuccessResponse(results, "");
    
    // 验证 ocrResults 数组
    ASSERT_EQ(response["result"]["ocrResults"].size(), 2);
    EXPECT_EQ(response["result"]["ocrResults"][0]["prunedResult"].get<std::string>(), "Hello World");
    EXPECT_EQ(response["result"]["ocrResults"][1]["prunedResult"].get<std::string>(), "Test Text");
}

/**
 * @brief 测试成功响应包含可视化 URL
 */
TEST(JsonResponseBuilder, BuildSuccessResponse_WithVisUrl) {
    std::vector<ocr::PipelineOCRResult> results;
    
    ocr::PipelineOCRResult result;
    result.text = "Test";
    result.confidence = 0.9f;
    result.box = {
        cv::Point2f(0, 0), cv::Point2f(10, 0),
        cv::Point2f(10, 10), cv::Point2f(0, 10)
    };
    results.push_back(result);
    
    std::string vis_url = "/static/vis/ocr_vis_12345.jpg";
    json response = JsonResponseBuilder::BuildSuccessResponse(results, vis_url);
    
    EXPECT_TRUE(response["result"].contains("ocrImage"));
    EXPECT_EQ(response["result"]["ocrImage"].get<std::string>(), vis_url);
}

/**
 * @brief 测试空结果的成功响应
 */
TEST(JsonResponseBuilder, BuildSuccessResponse_EmptyResults) {
    std::vector<ocr::PipelineOCRResult> results;  // 空结果
    
    json response = JsonResponseBuilder::BuildSuccessResponse(results, "");
    
    EXPECT_EQ(response["errorCode"].get<int>(), ErrorCode::SUCCESS);
    EXPECT_EQ(response["result"]["ocrResults"].size(), 0);
}

// ==================== BuildErrorResponse 测试 ====================

/**
 * @brief 测试错误响应的基本结构
 */
TEST(JsonResponseBuilder, BuildErrorResponse_Structure) {
    json response = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::INVALID_PARAMETER, "Test error message");
    
    // 验证必需字段存在
    EXPECT_TRUE(response.contains("logId"));
    EXPECT_TRUE(response.contains("errorCode"));
    EXPECT_TRUE(response.contains("errorMsg"));
    
    // 错误响应不应该包含 result 字段
    EXPECT_FALSE(response.contains("result"));
}

/**
 * @brief 测试错误响应的错误码
 */
TEST(JsonResponseBuilder, BuildErrorResponse_InvalidParameter) {
    json response = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::INVALID_PARAMETER, "Invalid parameter: file is empty");
    
    EXPECT_EQ(response["errorCode"].get<int>(), ErrorCode::INVALID_PARAMETER);
    EXPECT_EQ(response["errorMsg"].get<std::string>(), "Invalid parameter: file is empty");
}

/**
 * @brief 测试不同错误类型
 */
TEST(JsonResponseBuilder, BuildErrorResponse_DifferentErrorCodes) {
    // 未授权错误
    json unauthorized = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::UNAUTHORIZED, "Missing authorization token");
    EXPECT_EQ(unauthorized["errorCode"].get<int>(), ErrorCode::UNAUTHORIZED);
    
    // 内部错误
    json internal = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::INTERNAL_ERROR, "Internal server error");
    EXPECT_EQ(internal["errorCode"].get<int>(), ErrorCode::INTERNAL_ERROR);
    
    // 服务不可用
    json unavailable = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::SERVICE_UNAVAILABLE, "Service is busy");
    EXPECT_EQ(unavailable["errorCode"].get<int>(), ErrorCode::SERVICE_UNAVAILABLE);
    
    // 超时
    json timeout = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::TIMEOUT, "Request timed out");
    EXPECT_EQ(timeout["errorCode"].get<int>(), ErrorCode::TIMEOUT);
}

/**
 * @brief 测试错误响应包含有效的 logId
 */
TEST(JsonResponseBuilder, BuildErrorResponse_HasValidLogId) {
    json response = JsonResponseBuilder::BuildErrorResponse(
        ErrorCode::INTERNAL_ERROR, "Error");
    
    std::string logId = response["logId"].get<std::string>();
    EXPECT_EQ(logId.length(), 36);  // UUID 格式
}

// ==================== ConvertOCRResultToJson 测试 ====================

/**
 * @brief 测试 OCR 结果转 JSON 的基本结构
 */
TEST(JsonResponseBuilder, ConvertOCRResultToJson_Structure) {
    ocr::PipelineOCRResult result;
    result.text = "Test Text";
    result.confidence = 0.95f;
    result.box = {
        cv::Point2f(10.5f, 20.3f),
        cv::Point2f(100.7f, 20.8f),
        cv::Point2f(100.2f, 50.9f),
        cv::Point2f(10.1f, 50.4f)
    };
    
    json item = JsonResponseBuilder::ConvertOCRResultToJson(result, "");
    
    // 验证必需字段
    EXPECT_TRUE(item.contains("prunedResult"));
    EXPECT_TRUE(item.contains("score"));
    EXPECT_TRUE(item.contains("points"));
}

/**
 * @brief 测试 OCR 结果中的文本内容
 */
TEST(JsonResponseBuilder, ConvertOCRResultToJson_Text) {
    ocr::PipelineOCRResult result;
    result.text = "中文测试 English 123";
    result.confidence = 0.88f;
    result.box = {
        cv::Point2f(0, 0), cv::Point2f(10, 0),
        cv::Point2f(10, 10), cv::Point2f(0, 10)
    };
    
    json item = JsonResponseBuilder::ConvertOCRResultToJson(result, "");
    
    EXPECT_EQ(item["prunedResult"].get<std::string>(), "中文测试 English 123");
}

/**
 * @brief 测试置信度精度（保留3位小数）
 */
TEST(JsonResponseBuilder, ConvertOCRResultToJson_ScorePrecision) {
    ocr::PipelineOCRResult result;
    result.text = "Test";
    result.confidence = 0.123456f;
    result.box = {
        cv::Point2f(0, 0), cv::Point2f(10, 0),
        cv::Point2f(10, 10), cv::Point2f(0, 10)
    };
    
    json item = JsonResponseBuilder::ConvertOCRResultToJson(result, "");
    
    // 应该四舍五入到3位小数
    double score = item["score"].get<double>();
    EXPECT_NEAR(score, 0.123, 0.001);
}

/**
 * @brief 测试坐标点格式
 */
TEST(JsonResponseBuilder, ConvertOCRResultToJson_Points) {
    ocr::PipelineOCRResult result;
    result.text = "Test";
    result.confidence = 0.9f;
    result.box = {
        cv::Point2f(10.15f, 20.25f),
        cv::Point2f(100.35f, 20.45f),
        cv::Point2f(100.55f, 50.65f),
        cv::Point2f(10.75f, 50.85f)
    };
    
    json item = JsonResponseBuilder::ConvertOCRResultToJson(result, "");
    
    // 验证有4个点
    ASSERT_EQ(item["points"].size(), 4);
    
    // 验证每个点都有 x 和 y
    for (const auto& point : item["points"]) {
        EXPECT_TRUE(point.contains("x"));
        EXPECT_TRUE(point.contains("y"));
    }
    
    // 验证坐标精度（保留1位小数）
    EXPECT_NEAR(item["points"][0]["x"].get<double>(), 10.2, 0.1);
    EXPECT_NEAR(item["points"][0]["y"].get<double>(), 20.3, 0.1);
}

/**
 * @brief 测试包含可视化 URL
 */
TEST(JsonResponseBuilder, ConvertOCRResultToJson_WithVisUrl) {
    ocr::PipelineOCRResult result;
    result.text = "Test";
    result.confidence = 0.9f;
    result.box = {
        cv::Point2f(0, 0), cv::Point2f(10, 0),
        cv::Point2f(10, 10), cv::Point2f(0, 10)
    };
    
    std::string vis_url = "/static/vis/test.jpg";
    json item = JsonResponseBuilder::ConvertOCRResultToJson(result, vis_url);
    
    EXPECT_TRUE(item.contains("ocrImage"));
    EXPECT_EQ(item["ocrImage"].get<std::string>(), vis_url);
}

/**
 * @brief 测试不包含可视化 URL
 */
TEST(JsonResponseBuilder, ConvertOCRResultToJson_WithoutVisUrl) {
    ocr::PipelineOCRResult result;
    result.text = "Test";
    result.confidence = 0.9f;
    result.box = {
        cv::Point2f(0, 0), cv::Point2f(10, 0),
        cv::Point2f(10, 10), cv::Point2f(0, 10)
    };
    
    json item = JsonResponseBuilder::ConvertOCRResultToJson(result, "");
    
    // 当没有可视化 URL 时，不应该包含 ocrImage 字段
    EXPECT_FALSE(item.contains("ocrImage"));
}

// ==================== ErrorCode 常量测试 ====================

/**
 * @brief 验证错误码常量值
 */
TEST(ErrorCode, ConstantValues) {
    EXPECT_EQ(ErrorCode::SUCCESS, 0);
    EXPECT_EQ(ErrorCode::INVALID_PARAMETER, 400);
    EXPECT_EQ(ErrorCode::UNAUTHORIZED, 401);
    EXPECT_EQ(ErrorCode::INTERNAL_ERROR, 500);
    EXPECT_EQ(ErrorCode::SERVICE_UNAVAILABLE, 503);
    EXPECT_EQ(ErrorCode::TIMEOUT, 504);
}

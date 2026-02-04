#pragma once

#include "pipeline/ocr_pipeline.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace ocr_server {

/**
 * @brief JSON响应构建器
 */
class JsonResponseBuilder {
public:
    /**
     * @brief 生成UUID作为logId
     */
    static std::string GenerateUUID();
    
    /**
     * @brief 构建成功的OCR响应
     * @param results OCR识别结果列表
     * @param vis_image_url 可视化图片URL（可选）
     * @return JSON响应对象
     */
    static json BuildSuccessResponse(
        const std::vector<ocr::PipelineOCRResult>& results,
        const std::string& vis_image_url = ""
    );
    
    /**
     * @brief 构建错误响应
     * @param error_code 错误码
     * @param error_msg 错误信息
     * @return JSON响应对象
     */
    static json BuildErrorResponse(int error_code, const std::string& error_msg);
    
    /**
     * @brief 将PipelineOCRResult转换为JSON格式
     * @param result 单个OCR结果
     * @param vis_image_url 可视化图片URL（可选）
     * @return JSON对象
     */
    static json ConvertOCRResultToJson(
        const ocr::PipelineOCRResult& result,
        const std::string& vis_image_url = ""
    );
    
    /**
     * @brief 构建 PDF OCR 成功响应
     * @param pages_results 每页的 OCR 结果数组
     * @param totalPages PDF 总页数
     * @param renderedPages 实际渲染的页数
     * @return JSON 响应对象
     */
    static json BuildPDFSuccessResponse(
        const json& pages_results,
        int totalPages,
        int renderedPages
    );
};

/**
 * @brief HTTP错误码定义
 */
namespace ErrorCode {
    constexpr int SUCCESS = 0;
    constexpr int INVALID_PARAMETER = 400;
    constexpr int UNAUTHORIZED = 401;
    constexpr int INTERNAL_ERROR = 500;
    constexpr int SERVICE_UNAVAILABLE = 503;
    constexpr int TIMEOUT = 504;
}

} // namespace ocr_server

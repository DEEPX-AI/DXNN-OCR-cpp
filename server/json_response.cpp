#include "json_response.h"
#include "common/logger.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>

namespace ocr_server {

std::string JsonResponseBuilder::GenerateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    
    return std::string(uuid_str);
}

json JsonResponseBuilder::BuildSuccessResponse(
    const std::vector<ocr::PipelineOCRResult>& results,
    const std::string& vis_image_url) {
    
    json response;
    response["logId"] = GenerateUUID();
    response["errorCode"] = ErrorCode::SUCCESS;
    response["errorMsg"] = "Success";
    
    json ocr_results = json::array();
    for (const auto& result : results) {
        ocr_results.push_back(ConvertOCRResultToJson(result));
    }
    
    response["result"]["ocrResults"] = ocr_results;
    
    if (!vis_image_url.empty()) {
        response["result"]["ocrImage"] = vis_image_url;
    }
    
    return response;
}

json JsonResponseBuilder::BuildErrorResponse(int error_code, const std::string& error_msg) {
    json response;
    response["logId"] = GenerateUUID();
    response["errorCode"] = error_code;
    response["errorMsg"] = error_msg;
    
    return response;
}

json JsonResponseBuilder::ConvertOCRResultToJson(
    const ocr::PipelineOCRResult& result,
    const std::string& vis_image_url) {
    
    json item;
    
    // 核心字段：识别的文本内容
    item["prunedResult"] = result.text;
    
    // 置信度
    item["score"] = std::round(result.confidence * 1000.0) / 1000.0;  // 保留3位小数
    
    // 文本框坐标点（四个顶点）
    json points = json::array();
    for (const auto& pt : result.box) {
        json point;
        point["x"] = std::round(pt.x * 10.0) / 10.0;  // 保留1位小数
        point["y"] = std::round(pt.y * 10.0) / 10.0;
        points.push_back(point);
    }
    item["points"] = points;
    
    // 可视化图片URL（如果启用）
    if (!vis_image_url.empty()) {
        item["ocrImage"] = vis_image_url;
    }
    
    return item;
}

json JsonResponseBuilder::BuildPDFSuccessResponse(
    const json& pages_results,
    int totalPages,
    int renderedPages) {
    
    json response;
    response["logId"] = GenerateUUID();
    response["errorCode"] = ErrorCode::SUCCESS;
    response["errorMsg"] = "Success";
    
    json result;
    result["totalPages"] = totalPages;
    result["renderedPages"] = renderedPages;
    result["pages"] = pages_results;
    
    // 如果有页数被截断，添加警告
    if (renderedPages < totalPages) {
        result["warning"] = "Only first " + std::to_string(renderedPages) + 
                           " of " + std::to_string(totalPages) + 
                           " pages were processed due to page limit";
    }
    
    response["result"] = result;
    
    return response;
}

} // namespace ocr_server

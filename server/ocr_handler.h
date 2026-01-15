#pragma once

#include "pipeline/ocr_pipeline.h"
#include "file_handler.h"
#include "json_response.h"
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

using json = nlohmann::json;

namespace ocr_server {

/**
 * @brief OCR请求参数结构
 */
struct OCRRequest {
    std::string file;                       // Base64编码或URL
    int fileType = 1;                       // 1: 图像, 0: PDF(未实现)
    bool useDocOrientationClassify = false; // 文档方向矫正
    bool useDocUnwarping = false;           // 图片扭曲矫正
    bool useTextlineOrientation = false;    // 文本行方向矫正
    int textDetLimitSideLen = 64;           // 图像边长限制（接收但不实际使用）
    std::string textDetLimitType = "min";   // 边长限制类型: "min" 或 "max"（接收但不实际使用）
    double textDetThresh = 0.3;             // 检测像素阈值
    double textDetBoxThresh = 0.6;          // 检测框阈值
    double textDetUnclipRatio = 1.5;        // 检测扩张系数
    double textRecScoreThresh = 0.0;        // 识别置信度阈值
    bool visualize = false;                 // 是否开启可视化
    
    /**
     * @brief 从JSON解析请求参数
     */
    static OCRRequest FromJson(const json& j);
    
    /**
     * @brief 验证请求参数
     */
    bool Validate(std::string& error_msg) const;
};

/**
 * @brief OCR请求处理器
 */
class OCRHandler {
public:
    /**
     * @brief 构造函数
     * @param pipeline_config OCR Pipeline配置
     * @param vis_output_dir 可视化图片输出目录
     * @param vis_url_prefix 可视化图片URL前缀
     */
    OCRHandler(
        const ocr::OCRPipelineConfig& pipeline_config,
        const std::string& vis_output_dir = "output/vis",
        const std::string& vis_url_prefix = "/static/vis"
    );
    
    /**
     * @brief 处理OCR请求
     * @param request OCR请求参数
     * @param response_json 输出的JSON响应
     * @return HTTP状态码
     */
    int HandleRequest(const OCRRequest& request, json& response_json);
    
private:
    /**
     * @brief 从请求参数创建OCR Pipeline配置
     */
    ocr::OCRPipelineConfig CreatePipelineConfig(const OCRRequest& request) const;
    
    /**
     * @brief 加载输入图像（Base64或URL）
     */
    bool LoadInputImage(const OCRRequest& request, cv::Mat& image, std::string& error_msg);
    
    std::shared_ptr<ocr::OCRPipeline> base_pipeline_;  // 基础Pipeline实例
    ocr::OCRPipelineConfig base_config_;               // 基础配置
    std::string vis_output_dir_;                       // 可视化输出目录
    std::string vis_url_prefix_;                       // 可视化URL前缀
    
    // 并发结果存储（解决多请求结果错位问题）
    struct TaskResult {
        std::vector<ocr::PipelineOCRResult> results;
        cv::Mat processedImage;
    };
    std::map<int64_t, TaskResult> result_store_;       // task_id -> 结果
    std::mutex result_mutex_;                           // 保护 result_store_
    std::condition_variable result_cv_;                 // 通知等待的请求
    std::thread result_collector_thread_;               // 后台结果收集线程
    std::atomic<bool> collector_running_{false};        // 收集线程运行标志
    
    void StartResultCollector();                        // 启动结果收集线程
    void StopResultCollector();                         // 停止结果收集线程
    void ResultCollectorLoop();                         // 结果收集循环
    bool WaitForResult(int64_t task_id, std::vector<ocr::PipelineOCRResult>& results, 
                       cv::Mat& processedImage, int timeout_ms = 10000);
};

} // namespace ocr_server

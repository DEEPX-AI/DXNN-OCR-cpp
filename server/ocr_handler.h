#pragma once

#include "pipeline/ocr_pipeline.h"
#include "file_handler.h"
#include "json_response.h"
#include "pdf_handler.h"
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
    int fileType = 1;                       // 1: 图像, 0: PDF
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
    
    // 请求大小限制
    static constexpr size_t MAX_BASE64_SIZE = 50 * 1024 * 1024;     // 50MB Base64
    static constexpr size_t MAX_URL_LENGTH = 2048;                  // URL 长度限制
    
    // PDF 参数配置
    int pdfDpi = 150;                       // PDF 渲染 DPI (默认 150)
    int pdfMaxPages = 10;                   // 最大处理页数 (默认 10)
    
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
        bool success = true;  // 任务是否成功（false 表示检测/识别引擎异常）
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
                       cv::Mat& processedImage, bool& success, int timeout_ms = 10000);
    
    // ==================== PDF 处理相关 ====================
    
    PDFHandler pdf_handler_;                            // PDF 处理器
    
    /**
     * @brief 处理 PDF OCR 请求（并行提交到 pipeline）
     */
    int HandlePDFRequest(const OCRRequest& request, json& response_json);
    
    /**
     * @brief 处理图像 OCR 请求（原 HandleRequest 逻辑）
     */
    int HandleImageRequest(const OCRRequest& request, json& response_json);
    
    /**
     * @brief 生成唯一任务 ID
     */
    static int64_t GenerateTaskId();
    
    /**
     * @brief 保存可视化图片并返回 URL
     * @param image 处理后的图像
     * @param results OCR 结果
     * @param pageIndex 页码 (-1 表示非 PDF)
     */
    std::string SaveVisualization(const cv::Mat& image, 
                                   const std::vector<ocr::PipelineOCRResult>& results,
                                   int pageIndex = -1);
};

} // namespace ocr_server

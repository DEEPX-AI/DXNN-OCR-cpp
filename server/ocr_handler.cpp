#include "ocr_handler.h"
#include "common/logger.hpp"
#include "common/visualizer.h"
#include <regex>

namespace ocr_server {

// ==================== OCRRequest ====================

OCRRequest OCRRequest::FromJson(const json& j) {
    OCRRequest req;
    
    // 必填字段
    if (j.contains("file") && j["file"].is_string()) {
        req.file = j["file"].get<std::string>();
    }
    
    // 可选字段（使用默认值）
    if (j.contains("fileType")) req.fileType = j["fileType"].get<int>();
    if (j.contains("useDocOrientationClassify")) req.useDocOrientationClassify = j["useDocOrientationClassify"].get<bool>();
    if (j.contains("useDocUnwarping")) req.useDocUnwarping = j["useDocUnwarping"].get<bool>();
    if (j.contains("useTextlineOrientation")) req.useTextlineOrientation = j["useTextlineOrientation"].get<bool>();
    if (j.contains("textDetLimitSideLen")) req.textDetLimitSideLen = j["textDetLimitSideLen"].get<int>();
    if (j.contains("textDetLimitType")) req.textDetLimitType = j["textDetLimitType"].get<std::string>();
    if (j.contains("textDetThresh")) req.textDetThresh = j["textDetThresh"].get<double>();
    if (j.contains("textDetBoxThresh")) req.textDetBoxThresh = j["textDetBoxThresh"].get<double>();
    if (j.contains("textDetUnclipRatio")) req.textDetUnclipRatio = j["textDetUnclipRatio"].get<double>();
    if (j.contains("textRecScoreThresh")) req.textRecScoreThresh = j["textRecScoreThresh"].get<double>();
    if (j.contains("visualize")) req.visualize = j["visualize"].get<bool>();
    
    return req;
}

bool OCRRequest::Validate(std::string& error_msg) const {
    // 检查必填字段
    if (file.empty()) {
        error_msg = "Missing required parameter: 'file'";
        return false;
    }
    
    // 检查fileType
    if (fileType != 1) {
        error_msg = "Unsupported fileType: PDF is not implemented (fileType=0)";
        return false;
    }
    
    // textDetLimitSideLen 和 textDetLimitType: 接收但不实际使用，只做基本验证
    // 不会因为这两个参数的值而拒绝请求
    if (textDetLimitSideLen < 1) {
        LOG_WARN("textDetLimitSideLen={} is too small, will use default model selection", textDetLimitSideLen);
    }
    if (textDetLimitType != "min" && textDetLimitType != "max") {
        LOG_WARN("textDetLimitType='{}' is invalid (should be 'min' or 'max'), ignored", textDetLimitType);
    }
    
    // 检查实际使用的参数范围
    if (textDetThresh < 0.0 || textDetThresh > 1.0) {
        error_msg = "textDetThresh must be in range [0.0, 1.0]";
        return false;
    }
    
    if (textDetBoxThresh < 0.0 || textDetBoxThresh > 1.0) {
        error_msg = "textDetBoxThresh must be in range [0.0, 1.0]";
        return false;
    }
    
    if (textDetUnclipRatio < 1.0 || textDetUnclipRatio > 3.0) {
        error_msg = "textDetUnclipRatio must be in range [1.0, 3.0]";
        return false;
    }
    
    if (textRecScoreThresh < 0.0 || textRecScoreThresh > 1.0) {
        error_msg = "textRecScoreThresh must be in range [0.0, 1.0]";
        return false;
    }
    
    return true;
}

// ==================== OCRHandler ====================

OCRHandler::OCRHandler(
    const ocr::OCRPipelineConfig& pipeline_config,
    const std::string& vis_output_dir,
    const std::string& vis_url_prefix)
    : base_config_(pipeline_config)
    , vis_output_dir_(vis_output_dir)
    , vis_url_prefix_(vis_url_prefix) {
    
    // 创建基础Pipeline实例（会被每次请求的配置覆盖）
    base_pipeline_ = std::make_shared<ocr::OCRPipeline>(base_config_);
    LOG_INFO("OCRHandler initialized");
}

void OCRHandler::StartResultCollector() {
    if (collector_running_) return;
    collector_running_ = true;
    result_collector_thread_ = std::thread(&OCRHandler::ResultCollectorLoop, this);
    LOG_INFO("Result collector thread started");
}

void OCRHandler::StopResultCollector() {
    if (!collector_running_) return;
    collector_running_ = false;
    if (result_collector_thread_.joinable()) {
        result_collector_thread_.join();
    }
    LOG_INFO("Result collector thread stopped");
}

void OCRHandler::ResultCollectorLoop() {
    while (collector_running_) {
        std::vector<ocr::PipelineOCRResult> results;
        int64_t result_id;
        cv::Mat processed_image;
        
        if (base_pipeline_->getResult(results, result_id, &processed_image)) {
            // #region agent log
            LOG_INFO("[COLLECTOR] Got result for task_id={}, storing in map", result_id);
            // #endregion
            
            {
                std::lock_guard<std::mutex> lock(result_mutex_);
                result_store_[result_id] = TaskResult{std::move(results), std::move(processed_image)};
            }
            result_cv_.notify_all();  // 通知所有等待的请求
        }
    }
}

bool OCRHandler::WaitForResult(int64_t task_id, std::vector<ocr::PipelineOCRResult>& results, 
                                cv::Mat& processedImage, int timeout_ms) {
    std::unique_lock<std::mutex> lock(result_mutex_);
    
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    
    while (true) {
        auto it = result_store_.find(task_id);
        if (it != result_store_.end()) {
            // 找到结果
            results = std::move(it->second.results);
            processedImage = std::move(it->second.processedImage);
            result_store_.erase(it);
            // #region agent log
            LOG_INFO("[WAIT] Found result for task_id={}", task_id);
            // #endregion
            return true;
        }
        
        // 等待通知或超时
        if (result_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            LOG_WARN("[WAIT] Timeout waiting for task_id={}", task_id);
            return false;
        }
    }
}

ocr::OCRPipelineConfig OCRHandler::CreatePipelineConfig(const OCRRequest& request) const {
    ocr::OCRPipelineConfig config = base_config_;
    
    // 文档预处理配置
    config.useDocPreprocessing = request.useDocOrientationClassify || request.useDocUnwarping;
    config.docPreprocessingConfig.useOrientation = request.useDocOrientationClassify;
    config.docPreprocessingConfig.useUnwarping = request.useDocUnwarping;
    
    // 文本行方向分类
    config.useClassification = request.useTextlineOrientation;
    
    // 检测参数
    config.detectorConfig.sizeThreshold = request.textDetLimitSideLen;
    config.detectorConfig.thresh = static_cast<float>(request.textDetThresh);
    config.detectorConfig.boxThresh = static_cast<float>(request.textDetBoxThresh);
    config.detectorConfig.unclipRatio = static_cast<float>(request.textDetUnclipRatio);
    
    // 识别参数（需要在RecognizerConfig中添加scoreThresh字段）
    // config.recognizerConfig.scoreThresh = static_cast<float>(request.textRecScoreThresh);
    
    // 可视化
    config.enableVisualization = request.visualize;
    
    return config;
}

bool OCRHandler::LoadInputImage(const OCRRequest& request, cv::Mat& image, std::string& error_msg) {
    // 判断是Base64还是URL
    bool is_url = false;
    if (request.file.find("http://") == 0 || request.file.find("https://") == 0) {
        is_url = true;
    }
    
    if (is_url) {
        // 从URL下载
        LOG_INFO("Downloading image from URL...");
        if (!FileHandler::DownloadImageFromURL(request.file, image)) {
            error_msg = "Failed to download image from URL";
            return false;
        }
    } else {
        // Base64解码
        LOG_INFO("Decoding Base64 image...");
        if (!FileHandler::DecodeBase64Image(request.file, image)) {
            error_msg = "Failed to decode Base64 image";
            return false;
        }
    }
    
    return true;
}

int OCRHandler::HandleRequest(const OCRRequest& request, json& response_json) {
    try {
        // 1. 验证请求参数
        std::string error_msg;
        if (!request.Validate(error_msg)) {
            LOG_WARN("Invalid request: {}", error_msg);
            response_json = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INVALID_PARAMETER, error_msg);
            return 400;
        }
        
        // 2. 加载输入图像
        cv::Mat image;
        if (!LoadInputImage(request, image, error_msg)) {
            LOG_ERROR("Failed to load image: {}", error_msg);
            response_json = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INVALID_PARAMETER, error_msg);
            return 400;
        }
        
        LOG_INFO("Input image loaded: {}x{}", image.cols, image.rows);
        
        // 使用全局共享的 pipeline，避免每次请求都创建新实例
        // 如果 pipeline 未初始化，则初始化一次
        static std::once_flag init_flag;
        std::call_once(init_flag, [this]() {
            if (!base_pipeline_->initialize()) {
                LOG_ERROR("Failed to initialize base pipeline");
                throw std::runtime_error("Failed to initialize OCR pipeline");
            }
            base_pipeline_->start();  // 启动后保持运行
            LOG_INFO("Base pipeline initialized and started");
            
            // 启动结果收集线程（解决并发结果错位问题）
            StartResultCollector();
        });
        
        // 从 OCRRequest 构建 OCRTaskConfig（实现参数解耦）
        ocr::OCRTaskConfig taskConfig;
        taskConfig.useDocOrientationClassify = request.useDocOrientationClassify;
        taskConfig.useDocUnwarping = request.useDocUnwarping;
        taskConfig.useTextlineOrientation = request.useTextlineOrientation;
        taskConfig.textDetThresh = static_cast<float>(request.textDetThresh);
        taskConfig.textDetBoxThresh = static_cast<float>(request.textDetBoxThresh);
        taskConfig.textDetUnclipRatio = static_cast<float>(request.textDetUnclipRatio);
        taskConfig.textRecScoreThresh = static_cast<float>(request.textRecScoreThresh);
        
        LOG_INFO("OCRTaskConfig: docOri={}, docUnwarp={}, textlineOri={}, detThresh={:.2f}, boxThresh={:.2f}, unclipRatio={:.2f}, recThresh={:.2f}",
                 taskConfig.useDocOrientationClassify, taskConfig.useDocUnwarping,
                 taskConfig.useTextlineOrientation, taskConfig.textDetThresh,
                 taskConfig.textDetBoxThresh, taskConfig.textDetUnclipRatio, taskConfig.textRecScoreThresh);
        
        // 提交任务到共享 pipeline（使用新的带配置的接口）
        static std::atomic<int64_t> task_counter{0};
        int64_t task_id = ++task_counter;
        
        // #region agent log
        LOG_INFO("[DEBUG] Pushing task_id={}", task_id);
        // #endregion
        
        if (!base_pipeline_->pushTask(image, task_id, taskConfig)) {
            LOG_ERROR("Failed to push task to pipeline");

            response_json = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INTERNAL_ERROR, "Pipeline queue is full");
            return 503;
        }
        
        // 使用 WaitForResult 等待正确的结果（支持并发）
        std::vector<ocr::PipelineOCRResult> results;
        cv::Mat processed_image;
        
        LOG_INFO("Waiting for OCR results for task_id={}...", task_id);
        
        if (!WaitForResult(task_id, results, processed_image, 10000)) {
            LOG_ERROR("Failed to get OCR results for task_id={} (timeout)", task_id);
            response_json = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INTERNAL_ERROR, "Failed to get OCR results or timeout");
            return 500;
        }
        
        bool success = true;
        
        if (!success) {
            LOG_ERROR("OCR pipeline execution failed");
            response_json = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INTERNAL_ERROR, "OCR processing failed");
            return 500;
        }
        
        LOG_INFO("OCR completed: {} text boxes detected", results.size());
        
        // 5. 保存可视化图像（如果启用）
        std::string vis_url;
        LOG_INFO("Starting visualization check, visualize={}", request.visualize);
        if (request.visualize && !processed_image.empty()) {
            // 将PipelineOCRResult转换为TextBox以便使用Visualizer
            std::vector<ocr::TextBox> text_boxes;
            for (const auto& result : results) {
                ocr::TextBox box;
                // 复制4个顶点坐标
                for (size_t i = 0; i < 4 && i < result.box.size(); ++i) {
                    box.points[i] = result.box[i];
                }
                box.text = result.text;
                box.confidence = result.confidence;
                box.rotated = false;
                text_boxes.push_back(box);
            }
            
            // 使用可视化器生成带框的图像
            cv::Mat vis_image = ocr::Visualizer::drawOCRResults(
                processed_image, text_boxes, true, true);
            
            std::string vis_filename = FileHandler::SaveVisualizationImage(vis_image, vis_output_dir_);
            if (!vis_filename.empty()) {
                vis_url = vis_url_prefix_ + "/" + vis_filename;
                LOG_INFO("Visualization image saved: {}", vis_url);
            }
        }
        
        // 6. 构建成功响应
        LOG_INFO("Building success response...");
        response_json = JsonResponseBuilder::BuildSuccessResponse(results, vis_url);
        LOG_INFO("Success response built successfully");
        
        return 200;
        
    } catch (const json::exception& e) {
        LOG_ERROR("JSON parsing error: {}", e.what());
        response_json = JsonResponseBuilder::BuildErrorResponse(
            ErrorCode::INVALID_PARAMETER, std::string("Invalid JSON: ") + e.what());
        return 400;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in HandleRequest: {}", e.what());
        response_json = JsonResponseBuilder::BuildErrorResponse(
            ErrorCode::INTERNAL_ERROR, std::string("Internal error: ") + e.what());
        return 500;
    } catch (...) {
        LOG_ERROR("Unknown exception in HandleRequest");
        response_json = JsonResponseBuilder::BuildErrorResponse(
            ErrorCode::INTERNAL_ERROR, "Internal error: unknown exception");
        return 500;
    }
}

} // namespace ocr_server

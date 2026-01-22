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
    
    // PDF 专用参数
    if (j.contains("pdfDpi")) req.pdfDpi = j["pdfDpi"].get<int>();
    if (j.contains("pdfMaxPages")) req.pdfMaxPages = j["pdfMaxPages"].get<int>();
    
    return req;
}

bool OCRRequest::Validate(std::string& error_msg) const {
    // 检查必填字段
    if (file.empty()) {
        error_msg = "Missing required parameter: 'file'";
        return false;
    }
    
    // 检查文件大小限制
    bool is_url = (file.find("http://") == 0 || file.find("https://") == 0);
    
    if (is_url) {
        // URL 长度限制
        if (file.size() > MAX_URL_LENGTH) {
            error_msg = fmt::format("URL too long (max {} characters)", MAX_URL_LENGTH);
            return false;
        }
    } else {
        // Base64 大小限制
        if (file.size() > MAX_BASE64_SIZE) {
            error_msg = fmt::format("File too large (max {} MB)", 
                                    MAX_BASE64_SIZE / (1024 * 1024));
            return false;
        }
    }

    // 检查fileType
    if (fileType != 0 && fileType != 1) {
        error_msg = "fileType must be 0 (PDF) or 1 (Image)";
        return false;
    }
    
    // PDF 参数验证
    if (fileType == 0) {
        // DPI 限制 [72, 300]
        if (pdfDpi < 72 || pdfDpi > 300) {
            error_msg = "pdfDpi must be in range [72, 300]";
            return false;
        }
        
        // 页数限制 [1, 100]
        if (pdfMaxPages < 1 || pdfMaxPages > 100) {
            error_msg = "pdfMaxPages must be in range [1, 100]";
            return false;
        }
        
        // 内存预估警告（A4 @ 150 DPI ~= 8.7MB/页）
        if (pdfMaxPages > 10 && pdfDpi > 150) {
            LOG_WARN("High memory usage expected: {} pages at {} DPI", 
                     pdfMaxPages, pdfDpi);
        }
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
            LOG_DEBUG("[COLLECTOR] Got result for task_id={}, storing in map", result_id);
            
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
            LOG_DEBUG("[WAIT] Found result for task_id={}", task_id);
            return true;
        }
        
        // 等待通知或超时
        if (result_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            LOG_WARN("[WAIT] Timeout waiting for task_id={}", task_id);
            return false;
        }
    }
}

int64_t OCRHandler::GenerateTaskId() {
    static std::atomic<int64_t> task_counter{0};
    return ++task_counter;
}

std::string OCRHandler::SaveVisualization(const cv::Mat& image, 
                                           const std::vector<ocr::PipelineOCRResult>& results,
                                           int pageIndex) {
    if (image.empty()) return "";
    
    // 将 PipelineOCRResult 转换为 TextBox 以便使用 Visualizer
    std::vector<ocr::TextBox> text_boxes;
    for (const auto& result : results) {
        ocr::TextBox box;
        for (size_t i = 0; i < 4 && i < result.box.size(); ++i) {
            box.points[i] = result.box[i];
        }
        box.text = result.text;
        box.confidence = result.confidence;
        box.rotated = false;
        text_boxes.push_back(box);
    }
    
    // 使用可视化器生成带框的图像
    cv::Mat vis_image = ocr::Visualizer::drawOCRResults(image, text_boxes, true, true);
    
    // 生成文件名（支持页码后缀）
    std::string vis_filename;
    if (pageIndex >= 0) {
        // PDF 多页模式：添加页码后缀
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        vis_filename = fmt::format("ocr_vis_{}_page{}.jpg", timestamp, pageIndex);
        
        std::string full_path = vis_output_dir_ + "/" + vis_filename;
        if (cv::imwrite(full_path, vis_image)) {
            LOG_INFO("Visualization image saved: {}", full_path);
        } else {
            LOG_ERROR("Failed to save visualization image: {}", full_path);
            return "";
        }
    } else {
        // 单图模式：使用 FileHandler
        vis_filename = FileHandler::SaveVisualizationImage(vis_image, vis_output_dir_);
    }
    
    if (!vis_filename.empty()) {
        return vis_url_prefix_ + "/" + vis_filename;
    }
    return "";
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
        
        // 2. 确保 pipeline 已初始化
        static std::once_flag init_flag;
        std::call_once(init_flag, [this]() {
            if (!base_pipeline_->initialize()) {
                LOG_ERROR("Failed to initialize base pipeline");
                throw std::runtime_error("Failed to initialize OCR pipeline");
            }
            base_pipeline_->start();
            LOG_INFO("Base pipeline initialized and started");
            StartResultCollector();
        });
        
        // 3. 根据 fileType 分流处理
        if (request.fileType == 0) {
            // PDF 处理路径
            return HandlePDFRequest(request, response_json);
        } else {
            // 图像处理路径
            return HandleImageRequest(request, response_json);
        }
        
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

int OCRHandler::HandleImageRequest(const OCRRequest& request, json& response_json) {
    // 1. 加载输入图像
    std::string error_msg;
    cv::Mat image;
    if (!LoadInputImage(request, image, error_msg)) {
        LOG_ERROR("Failed to load image: {}", error_msg);
        response_json = JsonResponseBuilder::BuildErrorResponse(
            ErrorCode::INVALID_PARAMETER, error_msg);
        return 400;
    }
    
    LOG_INFO("Input image loaded: {}x{}", image.cols, image.rows);
    
    // 2. 构建 OCR 任务配置
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
    
    // 3. 提交任务到 pipeline
    int64_t task_id = GenerateTaskId();
    LOG_DEBUG("Pushing task_id={}", task_id);
        
    if (!base_pipeline_->pushTask(image, task_id, taskConfig)) {
        LOG_ERROR("Failed to push task to pipeline");
        response_json = JsonResponseBuilder::BuildErrorResponse(
            ErrorCode::INTERNAL_ERROR, "Pipeline queue is full");
        return 503;
    }
    
    // 4. 等待结果
    std::vector<ocr::PipelineOCRResult> results;
    cv::Mat processed_image;
    
    LOG_INFO("Waiting for OCR results for task_id={}...", task_id);
    
    if (!WaitForResult(task_id, results, processed_image, 10000)) {
        LOG_ERROR("Failed to get OCR results for task_id={} (timeout)", task_id);
        response_json = JsonResponseBuilder::BuildErrorResponse(
            ErrorCode::INTERNAL_ERROR, "Failed to get OCR results or timeout");
        return 500;
    }
    
    LOG_INFO("OCR completed: {} text boxes detected", results.size());
    
    // 5. 保存可视化图像（如果启用）
    std::string vis_url;
    if (request.visualize && !processed_image.empty()) {
        vis_url = SaveVisualization(processed_image, results);
        if (!vis_url.empty()) {
            LOG_INFO("Visualization image saved: {}", vis_url);
        }
    }
    
    // 6. 构建成功响应
    response_json = JsonResponseBuilder::BuildSuccessResponse(results, vis_url);
    return 200;
}

int OCRHandler::HandlePDFRequest(const OCRRequest& request, json& response_json) {
    LOG_INFO("Processing PDF request: dpi={}, maxPages={}", request.pdfDpi, request.pdfMaxPages);
    
    // 1. 构建 PDF 渲染配置
    PDFRenderConfig pdfConfig;
    pdfConfig.dpi = request.pdfDpi;
    pdfConfig.maxPages = request.pdfMaxPages;
    pdfConfig.maxDpi = 300;  // 硬限制
    
    // 2. 渲染 PDF 所有页面（内部已并行）
    PDFRenderResult renderResult;
    bool isURL = (request.file.find("http://") == 0 || request.file.find("https://") == 0);
    
    if (isURL) {
        LOG_INFO("Rendering PDF from URL...");
        renderResult = pdf_handler_.RenderFromURL(request.file, pdfConfig);
    } else {
        LOG_INFO("Rendering PDF from Base64...");
        renderResult = pdf_handler_.RenderFromBase64(request.file, pdfConfig);
    }
    
    // 3. 检查 PDF 渲染错误
    if (!renderResult.success && renderResult.pages.empty()) {
        LOG_ERROR("PDF rendering failed: {}", renderResult.errorMsg);
        response_json = JsonResponseBuilder::BuildErrorResponse(
            renderResult.errorCode, renderResult.errorMsg);
        return PDFHandler::GetHttpStatusCode(renderResult.errorCode);
    }
    
    LOG_INFO("PDF rendered: {} pages (total: {})", 
             renderResult.renderedPages, renderResult.totalPages);
    
    // 4. 构建 OCR 任务配置
    ocr::OCRTaskConfig taskConfig;
    taskConfig.useDocOrientationClassify = request.useDocOrientationClassify;
    taskConfig.useDocUnwarping = request.useDocUnwarping;
    taskConfig.useTextlineOrientation = request.useTextlineOrientation;
    taskConfig.textDetThresh = static_cast<float>(request.textDetThresh);
    taskConfig.textDetBoxThresh = static_cast<float>(request.textDetBoxThresh);
    taskConfig.textDetUnclipRatio = static_cast<float>(request.textDetUnclipRatio);
    taskConfig.textRecScoreThresh = static_cast<float>(request.textRecScoreThresh);
    
    // 5. 并行提交所有页面到 OCR pipeline
    struct PageTask {
        int64_t taskId;
        int pageIndex;
    };
    std::vector<PageTask> submittedTasks;
    
    for (const auto& page : renderResult.pages) {
        if (!page.success) {
            LOG_WARN("Skipping failed page {}", page.pageIndex);
            continue;
        }
        
        int64_t taskId = GenerateTaskId();
        
        if (base_pipeline_->pushTask(page.image, taskId, taskConfig)) {
            submittedTasks.push_back({taskId, page.pageIndex});
            LOG_DEBUG("Submitted page {} as task_id={}", page.pageIndex, taskId);
        } else {
            LOG_ERROR("Failed to submit page {} to pipeline (queue full)", page.pageIndex);
        }
    }
    
    // 6. 等待所有结果
    std::map<int, json> pageResults;      // pageIndex -> ocrResults
    std::map<int, std::string> pageVisUrls; // pageIndex -> vis_url
    
    for (const auto& task : submittedTasks) {
        std::vector<ocr::PipelineOCRResult> ocrResults;
        cv::Mat processedImage;
        
        if (WaitForResult(task.taskId, ocrResults, processedImage, 30000)) {
            // 构建该页的 OCR 结果 JSON
            json ocrResultsJson = json::array();
            for (const auto& r : ocrResults) {
                ocrResultsJson.push_back(JsonResponseBuilder::ConvertOCRResultToJson(r));
            }
            pageResults[task.pageIndex] = ocrResultsJson;
            
            LOG_INFO("Page {} OCR completed: {} text boxes", task.pageIndex, ocrResults.size());
            
            // 可视化（如果启用）
            if (request.visualize && !processedImage.empty()) {
                std::string visUrl = SaveVisualization(processedImage, ocrResults, task.pageIndex);
                if (!visUrl.empty()) {
                    pageVisUrls[task.pageIndex] = visUrl;
                }
            }
        } else {
            LOG_ERROR("Timeout waiting for page {} (task_id={})", task.pageIndex, task.taskId);
            pageResults[task.pageIndex] = json::array();  // 空结果
        }
    }
    
    // 7. 按页码顺序组装响应
    json pagesArray = json::array();
    for (int i = 0; i < renderResult.renderedPages; ++i) {
        json pageJson;
        pageJson["pageIndex"] = i;
        pageJson["ocrResults"] = pageResults.count(i) ? pageResults[i] : json::array();
        
        if (pageVisUrls.count(i)) {
            pageJson["ocrImage"] = pageVisUrls[i];
        }
        
        // 如果该页渲染失败，添加错误信息
        if (i < static_cast<int>(renderResult.pages.size()) && !renderResult.pages[i].success) {
            pageJson["error"] = renderResult.pages[i].errorMsg;
        }
        
        pagesArray.push_back(pageJson);
    }
    
    // 8. 构建最终响应
    response_json = JsonResponseBuilder::BuildPDFSuccessResponse(
        pagesArray, 
        renderResult.totalPages, 
        renderResult.renderedPages);
    
    LOG_INFO("PDF OCR completed: {} pages processed", renderResult.renderedPages);
    return 200;
}

} // namespace ocr_server

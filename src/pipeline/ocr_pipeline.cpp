#include "pipeline/ocr_pipeline.h"
#include "common/visualizer.h"
#include "common/geometry.h"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace ocr {

// ==================== OCRPipelineConfig ====================

void OCRPipelineConfig::Show() const {
    LOG_INFO("========== OCR Pipeline Configuration ==========");
    LOG_INFO("Detection Config:");
    detectorConfig.Show();
    
    if (useDocPreprocessing) {
        LOG_INFO("\nDocument Preprocessing Config:");
        docPreprocessingConfig.Show();
    }
    
    if (useClassification) {
        LOG_INFO("\nClassification Config:");
        classifierConfig.Show();
    }
    LOG_INFO("\nRecognition Config:");
    recognizerConfig.Show();
    LOG_INFO("\nPipeline Config:");
    LOG_INFO("  Use Document Preprocessing: {}", useDocPreprocessing ? "true" : "false");
    LOG_INFO("  Use Classification: {}", useClassification ? "true" : "false");
    LOG_INFO("  Enable Visualization: {}", enableVisualization ? "true" : "false");
    LOG_INFO("  Sort Results: {}", sortResults ? "true" : "false");
    LOG_INFO("===============================================");
}

// ==================== OCRResult ====================

cv::Rect PipelineOCRResult::getBoundingRect() const {
    if (box.size() != 4) {
        return cv::Rect();
    }
    
    float min_x = box[0].x, max_x = box[0].x;
    float min_y = box[0].y, max_y = box[0].y;
    
    for (size_t i = 1; i < 4; ++i) {
        min_x = std::min(min_x, box[i].x);
        max_x = std::max(max_x, box[i].x);
        min_y = std::min(min_y, box[i].y);
        max_y = std::max(max_y, box[i].y);
    }
    
    return cv::Rect(static_cast<int>(min_x), static_cast<int>(min_y),
                    static_cast<int>(max_x - min_x), static_cast<int>(max_y - min_y));
}

cv::Point2f PipelineOCRResult::getCenter() const {
    if (box.size() != 4) {
        return cv::Point2f(0, 0);
    }
    
    float center_x = 0, center_y = 0;
    for (const auto& pt : box) {
        center_x += pt.x;
        center_y += pt.y;
    }
    
    return cv::Point2f(center_x / 4.0f, center_y / 4.0f);
}

// ==================== OCRPipelineStats ====================

void OCRPipelineStats::Show() const {
    LOG_INFO("========== OCR Pipeline Statistics ==========");
    
    if (docPreprocessingTime > 0) {
        LOG_INFO("Doc Preprocessing: {:.2f} ms ({:.1f}%)", docPreprocessingTime,
                 totalTime > 0 ? docPreprocessingTime/totalTime*100 : 0);
    }

    LOG_INFO("Detection:");
    LOG_INFO("  Preprocess:  {:.2f} ms ({:.1f}%)", detectionPreprocessTime, 
             totalTime > 0 ? detectionPreprocessTime/totalTime*100 : 0);
    LOG_INFO("  Inference:   {:.2f} ms ({:.1f}%)", detectionInferenceTime,
             totalTime > 0 ? detectionInferenceTime/totalTime*100 : 0);
    LOG_INFO("  Postprocess: {:.2f} ms ({:.1f}%)", detectionPostprocessTime,
             totalTime > 0 ? detectionPostprocessTime/totalTime*100 : 0);
    LOG_INFO("  Total:       {:.2f} ms ({:.1f}%)", detectionTime,
             totalTime > 0 ? detectionTime/totalTime*100 : 0);
    
    LOG_INFO("Classification:");
    LOG_INFO("  Preprocess:  {:.2f} ms ({:.1f}%)", classificationPreprocessTime,
             totalTime > 0 ? classificationPreprocessTime/totalTime*100 : 0);
    LOG_INFO("  Inference:   {:.2f} ms ({:.1f}%)", classificationInferenceTime,
             totalTime > 0 ? classificationInferenceTime/totalTime*100 : 0);
    LOG_INFO("  Postprocess: {:.2f} ms ({:.1f}%)", classificationPostprocessTime,
             totalTime > 0 ? classificationPostprocessTime/totalTime*100 : 0);
    LOG_INFO("  Total:       {:.2f} ms ({:.1f}%)", classificationTime,
             totalTime > 0 ? classificationTime/totalTime*100 : 0);
    
    LOG_INFO("Recognition:");
    LOG_INFO("  Preprocess:  {:.2f} ms ({:.1f}%)", recognitionPreprocessTime,
             totalTime > 0 ? recognitionPreprocessTime/totalTime*100 : 0);
    LOG_INFO("  Inference:   {:.2f} ms ({:.1f}%)", recognitionInferenceTime,
             totalTime > 0 ? recognitionInferenceTime/totalTime*100 : 0);
    LOG_INFO("  Postprocess: {:.2f} ms ({:.1f}%)", recognitionPostprocessTime,
             totalTime > 0 ? recognitionPostprocessTime/totalTime*100 : 0);
    LOG_INFO("  Total:       {:.2f} ms ({:.1f}%)", recognitionTime,
             totalTime > 0 ? recognitionTime/totalTime*100 : 0);
    
    LOG_INFO("Total Time: {:.2f} ms", totalTime);
    LOG_INFO("Detected: {} | Rotated: {} | Recognized: {} ({:.1f}%)", 
             detectedBoxes, rotatedBoxes, recognizedBoxes, recognitionRate);
    LOG_INFO("============================================");
}

// ==================== OCRPipeline ====================

OCRPipeline::OCRPipeline(const OCRPipelineConfig& config)
    : config_(config), initialized_(false) {
    // Get CPU core count for thread pool sizing
    unsigned int numCores = std::thread::hardware_concurrency();
    if (numCores == 0) numCores = 4; // Fallback

    // Use only 1 recognition thread to avoid DevicePool lock contention
    // Multiple threads calling classifier/recognizer cause severe lock contention
    // on dxrt::DevicePool::PickOneDevice mutex
    numDetectionThreads_ = 1;  // Detection uses async callback, 1 is enough
    numRecognitionThreads_ = 1;  // Avoid lock contention
    
    // Initialize stage executor thread pool (similar to Python's ThreadPoolExecutor)
    // This is used to dispatch heavy work from DXRT callbacks to separate threads
    constexpr size_t STAGE_EXECUTOR_THREADS = 8;  // Similar to Python's max_workers=16
    stageExecutor_ = std::make_unique<ThreadPool>(STAGE_EXECUTOR_THREADS);
    
    LOG_INFO("OCRPipeline: Detected {} CPU cores", numCores);
    LOG_INFO("  Detection threads: {}", numDetectionThreads_);
    LOG_INFO("  Recognition threads: {}", numRecognitionThreads_);
    LOG_INFO("  Stage executor threads: {}", STAGE_EXECUTOR_THREADS);
}

OCRPipeline::~OCRPipeline() {
}

bool OCRPipeline::initialize() {
    if (initialized_) {
        LOG_WARN("OCRPipeline already initialized");
        return true;
    }
    
    LOG_INFO("Initializing OCR Pipeline...");
    
    // 初始化Detector
    detector_ = std::make_unique<TextDetector>(config_.detectorConfig);
    
    // Set callback for async mode
    // Detection callback is kept lightweight - it only dispatches to stageExecutor_
    detector_->setCallback([this](std::vector<DeepXOCR::TextBox> boxes, int64_t taskId, cv::Mat image, double /*pp*/, double /*inf*/, double /*post*/) {
        LOG_INFO("Detection callback: taskId={}, boxes={}", taskId, boxes.size());
        
        // Dispatch heavy work (sorting, queue push) to stageExecutor_
        // This avoids blocking DXRT internal callback thread
        stageExecutor_->dispatch([this, boxes = std::move(boxes), taskId, image]() mutable {
            // 从 map 中获取并移除任务配置
            OCRTaskConfig taskConfig;
            {
                std::lock_guard<std::mutex> lock(pendingTaskConfigsMutex_);
                auto it = pendingTaskConfigs_.find(taskId);
                if (it != pendingTaskConfigs_.end()) {
                    taskConfig = it->second;
                    pendingTaskConfigs_.erase(it);
                } else {
                    LOG_WARN("Task config not found for taskId={}, using default", taskId);
                }
            }
            
            // Sort Boxes
            std::sort(boxes.begin(), boxes.end(), [](const DeepXOCR::TextBox& a, const DeepXOCR::TextBox& b) {
                if (std::abs(a.points[0].y - b.points[0].y) < 1.0f) {
                    return a.points[0].x < b.points[0].x;
                }
                return a.points[0].y < b.points[0].y;
            });
            
            for (size_t i = 0; i < boxes.size() - 1; ++i) {
                for (int j = i; j >= 0; --j) {
                    if (std::abs(boxes[j + 1].points[0].y - boxes[j].points[0].y) < 10.0f &&
                        boxes[j + 1].points[0].x < boxes[j].points[0].x) {
                        std::swap(boxes[j], boxes[j + 1]);
                    } else {
                        break;
                    }
                }
            }

            // Push to Recognition Queue (non-blocking to avoid deadlock)
            // Check both running_ and recQueue_ existence atomically
            if (running_ && recQueue_) {
                size_t boxCount = boxes.size();
                RecognitionTask task{image, std::move(boxes), taskId, taskConfig};
                // Use try_push with longer timeout to avoid blocking callback threads
                while (running_ && recQueue_ && !recQueue_->try_push(std::move(task), std::chrono::milliseconds(500))) {
                    LOG_WARN("Recognition queue full, waiting... id={}", taskId);
                }
                if (running_ && recQueue_) {
                    LOG_INFO("Pushed task to recognition queue, id={}, boxes={}", taskId, boxCount);
                }
            } else {
                LOG_WARN("Pipeline stopping, discarding detection callback for taskId={}", taskId);
            }
        });
    });

    if (!detector_->init()) {
        LOG_ERROR("Failed to initialize TextDetector");
        return false;
    }
    
    // 初始化Document Preprocessing Pipeline（统一管理）
    if (config_.useDocPreprocessing) {
        docPreprocessing_ = std::make_unique<DocumentPreprocessingPipeline>(
            config_.docPreprocessingConfig);
        if (!docPreprocessing_->Initialize()) {
            LOG_WARN("Failed to initialize DocumentPreprocessingPipeline, proceeding without it");
            docPreprocessing_ = nullptr;
            config_.useDocPreprocessing = false;
        } else {
            LOG_INFO("DocumentPreprocessingPipeline initialized");
        }
    }
    
    // 初始化Classifier（可选）
    if (config_.useClassification) {
        classifier_ = std::make_unique<TextClassifier>(config_.classifierConfig);
        if (!classifier_->Initialize()) {
            LOG_ERROR("Failed to initialize TextClassifier");
            return false;
        }
        // Register async callback for classification (for pipelined cls->rec)
        classifier_->RegisterCallback([this](const std::string& label, float confidence, void* userArg) {
            this->onClassificationComplete(label, confidence, userArg);
        });
        LOG_INFO("Text Classifier enabled with async callback");
    } else {
        LOG_INFO("Text Classifier disabled");
    }
    
    // 初始化Recognizer
    recognizer_ = std::make_unique<TextRecognizer>(config_.recognizerConfig);
    if (!recognizer_->Initialize()) {
        LOG_ERROR("Failed to initialize TextRecognizer");
        return false;
    }
    
    // Register async callback for recognition
    recognizer_->RegisterCallback([this](const std::string& text, float confidence, void* userArg) {
        this->onRecognitionComplete(text, confidence, userArg);
    });
    LOG_INFO("Recognition async callback registered");
    
    initialized_ = true;
    LOG_INFO("✅ OCR Pipeline initialized successfully!\n");
    
    return true;
}

bool OCRPipeline::saveResultsToJSON(const std::vector<PipelineOCRResult>& results,
                                   const std::string& jsonPath) {
    std::ofstream ofs(jsonPath);
    if (!ofs.is_open()) {
        LOG_ERROR("Failed to open file for writing: %s", jsonPath.c_str());
        return false;
    }
    
    ofs << "{\n";
    ofs << "  \"results\": [\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        
        ofs << "    {\n";
        ofs << "      \"index\": " << result.index << ",\n";
        ofs << "      \"text\": \"" << result.text << "\",\n";
        ofs << "      \"confidence\": " << std::fixed << std::setprecision(4) 
            << result.confidence << ",\n";
        ofs << "      \"box\": [\n";
        
        for (size_t j = 0; j < result.box.size(); ++j) {
            ofs << "        [" << std::fixed << std::setprecision(2) 
                << result.box[j].x << ", " << result.box[j].y << "]";
            if (j < result.box.size() - 1) ofs << ",";
            ofs << "\n";
        }
        
        ofs << "      ]\n";
        ofs << "    }";
        if (i < results.size() - 1) ofs << ",";
        ofs << "\n";
    }
    
    ofs << "  ],\n";
    ofs << "  \"total_count\": " << results.size() << "\n";
    ofs << "}\n";
    
    ofs.close();
    LOG_INFO("Results saved to: {}", jsonPath.c_str());
    
    return true;
}

void OCRPipeline::sortOCRResults(std::vector<PipelineOCRResult>& results) {
    std::sort(results.begin(), results.end(), compareOCRResults);
}

bool OCRPipeline::compareOCRResults(const PipelineOCRResult& a, const PipelineOCRResult& b) {
    // 从上到下，从左到右排序
    // 1. 先按Y坐标分组（同一行的框Y坐标相近）
    // 2. 同一行内按X坐标排序
    
    cv::Point2f center_a = a.getCenter();
    cv::Point2f center_b = b.getCenter();
    
    // 定义行高阈值（如果两个框的Y坐标差距小于此值，认为在同一行）
    float row_threshold = std::min(a.getBoundingRect().height, 
                                   b.getBoundingRect().height) * 0.5f;
    
    float y_diff = std::abs(center_a.y - center_b.y);
    
    if (y_diff < row_threshold) {
        // 同一行，按X坐标排序
        return center_a.x < center_b.x;
    } else {
        // 不同行，按Y坐标排序
        return center_a.y < center_b.y;
    }
}

// ==================== Async Pipeline Implementation ====================

void OCRPipeline::start() {
    if (running_) {
        LOG_WARN("Pipeline already running");
        return;
    }

    // Use larger queue sizes to reduce backpressure
    detQueue_ = std::make_unique<ConcurrentQueue<DetectionTask>>(100);
    recQueue_ = std::make_unique<ConcurrentQueue<RecognitionTask>>(100);
    outQueue_ = std::make_unique<ConcurrentQueue<OutputTask>>(100);

    running_ = true;
    
    // Start multiple detection threads
    detThreads_.reserve(numDetectionThreads_);
    for (int i = 0; i < numDetectionThreads_; ++i) {
        detThreads_.emplace_back(&OCRPipeline::detectionLoop, this);
        LOG_INFO("Started detection thread {}/{}", i + 1, numDetectionThreads_);
    }
    
    // Start multiple recognition threads
    recThreads_.reserve(numRecognitionThreads_);
    for (int i = 0; i < numRecognitionThreads_; ++i) {
        recThreads_.emplace_back(&OCRPipeline::recognitionLoop, this);
        LOG_INFO("Started recognition thread {}/{}", i + 1, numRecognitionThreads_);
    }
    
    LOG_INFO("Async pipeline started: {} detection + {} recognition threads", 
             numDetectionThreads_, numRecognitionThreads_);
}

void OCRPipeline::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Push dummy tasks to unblock all threads (use try_push to avoid blocking)
    for (int i = 0; i < numDetectionThreads_; ++i) {
        if (detQueue_) detQueue_->try_push({}, std::chrono::milliseconds(100));
    }
    for (int i = 0; i < numRecognitionThreads_; ++i) {
        if (recQueue_) recQueue_->try_push({}, std::chrono::milliseconds(100));
    }
    
    // Join all detection threads
    for (auto& thread : detThreads_) {
        if (thread.joinable()) thread.join();
    }
    detThreads_.clear();
    
    // Join all recognition threads
    for (auto& thread : recThreads_) {
        if (thread.joinable()) thread.join();
    }
    recThreads_.clear();
    
    if (detQueue_) detQueue_->clear();
    if (recQueue_) recQueue_->clear();
    if (outQueue_) outQueue_->clear();
    
    LOG_INFO("Async pipeline stopped");
}

bool OCRPipeline::pushTask(const cv::Mat& image, int64_t id) {
    // 使用默认配置调用重载版本
    return pushTask(image, id, OCRTaskConfig::Default());
}

bool OCRPipeline::pushTask(const cv::Mat& image, int64_t id, const OCRTaskConfig& config) {
    if (!running_ || !detQueue_) return false;
    // Use try_push to avoid blocking - return false if queue is full
    if (!detQueue_->try_push({image, id, config}, std::chrono::milliseconds(100))) {
        return false;  // Queue full, caller should retry
    }
    LOG_INFO("Task pushed to detection queue, id={}, config: docOri={}, docUnwarp={}, textlineOri={}, detThresh={:.2f}, boxThresh={:.2f}, unclipRatio={:.2f}, recThresh={:.2f}",
             id, config.useDocOrientationClassify, config.useDocUnwarping, 
             config.useTextlineOrientation, config.textDetThresh, 
             config.textDetBoxThresh, config.textDetUnclipRatio, config.textRecScoreThresh);
    return true;
}

bool OCRPipeline::getResult(std::vector<PipelineOCRResult>& results, int64_t& id, cv::Mat* processedImage) {
    if (!running_ || !outQueue_) return false;
    
    OutputTask task;
    if (!outQueue_->try_pop(task, std::chrono::milliseconds(100))) {
        return false;
    }
    
    results = std::move(task.results);
    id = task.id;
    if (processedImage) {
        *processedImage = task.processedImage;
    }
    return true;
}

void OCRPipeline::detectionLoop() {
    while (running_) {
        DetectionTask task;
        if (!detQueue_->try_pop(task, std::chrono::milliseconds(100))) {
            LOG_DEBUG("Detection queue pop timeout, retrying...");
            continue;  // Timeout, check running_ and retry
        }
        LOG_INFO("Task popped from detection queue, id={}", task.id);
        if (!running_) break;
        if (task.image.empty()) continue;

        // 存储任务配置到 map 中（用于在检测回调中传递给识别阶段）
        {
            std::lock_guard<std::mutex> lock(pendingTaskConfigsMutex_);
            pendingTaskConfigs_[task.id] = task.config;
        }

        // 1. Doc Preprocessing (Doc Ori + UVDoc) - 根据 task.config 控制
        auto t1 = std::chrono::high_resolution_clock::now();
        cv::Mat processedImage = task.image;
        
        // 使用 task.config 控制是否进行文档预处理
        bool useDocPreproc = (task.config.useDocOrientationClassify || task.config.useDocUnwarping);
        if (useDocPreproc && docPreprocessing_) {
            // 动态设置文档预处理配置
            DocumentPreprocessingConfig dynamicConfig;
            dynamicConfig.useOrientation = task.config.useDocOrientationClassify;
            dynamicConfig.useUnwarping = task.config.useDocUnwarping;
            
            auto preprocResult = docPreprocessing_->Process(task.image, dynamicConfig);
            if (preprocResult.success && !preprocResult.processedImage.empty()) {
                processedImage = preprocResult.processedImage;
                LOG_DEBUG("Doc preprocessing applied: ori={}, unwarp={}", 
                          task.config.useDocOrientationClassify, task.config.useDocUnwarping);
            }
        }

        // 2. Detection Preprocess
        int resized_h, resized_w;
        int h = processedImage.rows;
        int w = processedImage.cols;
        
        int target_size = detector_->getTargetSize(h, w);

        cv::Mat preprocessed = detector_->preprocessAsync(processedImage, target_size, resized_h, resized_w);
        auto t2 = std::chrono::high_resolution_clock::now();
        double preprocess_time = std::chrono::duration<double, std::milli>(t2 - t1).count();

        // 3. Submit Async Inference（使用 task.config 中的检测参数）
        detector_->runAsync(preprocessed, h, w, resized_h, resized_w, task.id, processedImage, preprocess_time,
                            task.config.textDetThresh, task.config.textDetBoxThresh, task.config.textDetUnclipRatio);
    }
}

void OCRPipeline::recognitionLoop() {
    while (running_) {
        RecognitionTask task;
        if (!recQueue_->try_pop(task, std::chrono::milliseconds(100))) {
            LOG_DEBUG("Recognition queue pop timeout, retrying...");
            continue;  // Timeout, check running_ and retry
        }
        LOG_INFO("Task popped from recognition queue, id={}", task.id);

        if (!running_) break;
        if (task.boxes.empty()) {
            // No boxes detected, push empty result
            if (outQueue_) {
                outQueue_->push({std::vector<PipelineOCRResult>{}, task.image, task.id, task.config});
                LOG_INFO("Pushed empty result to output queue, id={}", task.id);
            }
            continue;
        }

        // ============================================================
        // OPTIMIZATION: Interleaved Crop & Submit
        // Instead of: [crop all] → [submit all]
        // Now:        [crop 1 → submit 1] → [crop 2 → submit 2] → ...
        // This allows NPU to start processing while CPU continues cropping
        // ============================================================
        
        size_t validBoxCount = task.boxes.size();
        auto taskCtx = std::make_shared<RecognitionTaskContext>(task.id, validBoxCount);
        taskCtx->processedImage = task.image.clone();  // 保存处理后的图像用于可视化
        
        LOG_INFO("Starting interleaved crop & submit for {} boxes, id={}, cls={}", 
                 validBoxCount, task.id, 
                 config_.useClassification ? "async" : "disabled");

        // Crop and submit immediately (interleaved)
        // Each crop is submitted to NPU right after it's created
        size_t actualCropIndex = 0;
        size_t failedCrops = 0;
        
        for (size_t i = 0; i < task.boxes.size(); ++i) {
            std::vector<cv::Point2f> box_points(4);
            for (int j = 0; j < 4; ++j) box_points[j] = task.boxes[i].points[j];
            
            // Crop this single box
            cv::Mat textImage = Geometry::getRotateCropImage(task.image, box_points);
            
            if (textImage.empty()) {
                // This crop failed, decrement pending count
                ++failedCrops;
                int remaining = taskCtx->pendingCount.fetch_sub(1) - 1;
                LOG_DEBUG("Crop {} failed (empty), remaining pending={}", i, remaining);
                
                // Check if all "crops" have been processed (all failed)
                if (remaining == 0) {
                    finalizeRecognitionTask(taskCtx);
                }
                continue;
            }
            
            // Store crop and box points in context
            taskCtx->crops[actualCropIndex] = std::move(textImage);
            taskCtx->boxPoints[actualCropIndex] = std::move(box_points);
            taskCtx->results[actualCropIndex].box = taskCtx->boxPoints[actualCropIndex];
            taskCtx->results[actualCropIndex].index = static_cast<int>(actualCropIndex);
            
            // IMMEDIATELY submit to classification/recognition pipeline
            // NPU starts processing while CPU continues to crop next box
            if (config_.useClassification && classifier_) {
                ClassificationCropContext* clsCtx = new ClassificationCropContext{taskCtx, actualCropIndex};
                classifier_->ClassifyAsync(taskCtx->crops[actualCropIndex], clsCtx);
            } else {
                submitCropForRecognition(taskCtx, actualCropIndex);
            }
            
            ++actualCropIndex;
        }
        
        LOG_DEBUG("Interleaved submission complete: {} valid crops, {} failed, id={}", 
                  actualCropIndex, failedCrops, task.id);
    }
}

// Helper: Submit a single crop for recognition (after classification or directly)
void OCRPipeline::submitCropForRecognition(std::shared_ptr<RecognitionTaskContext> taskCtx, size_t cropIndex) {
    const cv::Mat& crop = taskCtx->crops[cropIndex];
    
    // Submit async recognition (model will handle all ratios including long text via ratio_35)
    RecognitionCropContext* cropCtx = new RecognitionCropContext{taskCtx, cropIndex};
    recognizer_->RecognizeAsync(crop, cropCtx);
}

void OCRPipeline::onClassificationComplete(const std::string& label, float confidence, void* userArg) {
    ClassificationCropContext* clsCtx = static_cast<ClassificationCropContext*>(userArg);
    if (!clsCtx) {
        LOG_DEBUG("Classification callback: null context (sync call, ignoring)");
        return;
    }
    
    // Extract context data (lightweight)
    auto taskCtx = clsCtx->taskCtx;  // shared_ptr copy
    size_t idx = clsCtx->cropIndex;
    bool needsRotation = classifier_->NeedsRotation(label, confidence);
    
    // Clean up the raw pointer
    delete clsCtx;
    
    LOG_DEBUG("Classification complete for crop {} of task {}, label='{}', conf={:.3f}",
              idx, taskCtx->taskId, label, confidence);
    
    // Dispatch heavy work to thread pool (similar to Python's _dispatch_stage)
    // This avoids blocking DXRT internal callback thread
    stageExecutor_->dispatch([this, taskCtx, idx, needsRotation]() {
        // Rotate image if needed (in-place on taskCtx->crops)
        if (needsRotation) {
            cv::rotate(taskCtx->crops[idx], taskCtx->crops[idx], cv::ROTATE_180);
            LOG_DEBUG("Rotated crop {} by 180 degrees", idx);
        }
        
        // Submit to recognition (pipelined)
        submitCropForRecognition(taskCtx, idx);
    });
}

void OCRPipeline::onRecognitionComplete(const std::string& text, float confidence, void* userArg) {
    RecognitionCropContext* cropCtx = static_cast<RecognitionCropContext*>(userArg);
    if (!cropCtx) {
        LOG_DEBUG("Recognition callback: null crop context (sync call, ignoring)");
        return;
    }

    // Extract context data (lightweight)
    auto taskCtx = cropCtx->taskCtx;  // shared_ptr copy
    size_t idx = cropCtx->cropIndex;
    std::string textCopy = text;  // Copy text for dispatch
    
    // Clean up the raw pointer
    delete cropCtx;

    // Dispatch to thread pool to avoid blocking DXRT callback thread
    stageExecutor_->dispatch([this, taskCtx, idx, textCopy = std::move(textCopy), confidence]() {
        // Update result for this crop
        {
            std::lock_guard<std::mutex> lock(taskCtx->resultMutex);
            taskCtx->results[idx].text = textCopy;
            taskCtx->results[idx].confidence = confidence;
        }

        // Decrement pending count
        int remaining = taskCtx->pendingCount.fetch_sub(1) - 1;
        
        LOG_DEBUG("Recognition complete for crop {} of task {}, remaining={}, text='{}'",
                  idx, taskCtx->taskId, remaining, textCopy.empty() ? "<empty>" : textCopy.substr(0, 20));

        // If all crops done, finalize and output
        if (remaining == 0) {
            finalizeRecognitionTask(taskCtx);
        }
    });
}

void OCRPipeline::finalizeRecognitionTask(std::shared_ptr<RecognitionTaskContext> taskCtx) {
    // 获取识别置信度阈值
    float recScoreThresh = taskCtx->config.textRecScoreThresh;
    
    // Collect valid results (non-empty text and confidence >= threshold)
    std::vector<PipelineOCRResult> validResults;
    validResults.reserve(taskCtx->results.size());
    
    size_t filteredByThresh = 0;
    for (const auto& res : taskCtx->results) {
        if (!res.text.empty()) {
            // 使用 textRecScoreThresh 过滤低置信度的结果
            if (res.confidence >= recScoreThresh) {
                validResults.push_back(res);
            } else {
                ++filteredByThresh;
                LOG_DEBUG("Filtered result by threshold: conf={:.3f} < thresh={:.3f}, text='{}'",
                          res.confidence, recScoreThresh, res.text.substr(0, 20));
            }
        }
    }
    
    if (filteredByThresh > 0) {
        LOG_INFO("Filtered {} results by textRecScoreThresh={:.2f}", filteredByThresh, recScoreThresh);
    }

    // Sort results
    if (config_.sortResults && !validResults.empty()) {
        sortOCRResults(validResults);
        for (size_t i = 0; i < validResults.size(); ++i) {
            validResults[i].index = static_cast<int>(i);
        }
    }

    // Push to output queue (use try_push to avoid deadlock)
    // 传递 task config 到 output
    if (outQueue_ && running_) {
        size_t resultCount = validResults.size();  // Save before move
        while (running_ && !outQueue_->try_push({std::move(validResults), taskCtx->processedImage, taskCtx->taskId, taskCtx->config}, 
                                                 std::chrono::milliseconds(500))) {
            LOG_WARN("Output queue full, waiting... id={}", taskCtx->taskId);
        }
        if (running_) {
            LOG_INFO("Pushed result to output queue, id={}, results={}", 
                     taskCtx->taskId, resultCount);
        }
    }
}

} // namespace ocr

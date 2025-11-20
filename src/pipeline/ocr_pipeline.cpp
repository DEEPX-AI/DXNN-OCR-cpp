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
    LOG_INFO("  Use Document Preprocessing: %s", useDocPreprocessing ? "true" : "false");
    LOG_INFO("  Use Classification: %s", useClassification ? "true" : "false");
    LOG_INFO("  Enable Visualization: %s", enableVisualization ? "true" : "false");
    LOG_INFO("  Sort Results: %s", sortResults ? "true" : "false");
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
        LOG_INFO("Doc Preprocessing: %.2f ms (%.1f%%)", docPreprocessingTime,
                 totalTime > 0 ? docPreprocessingTime/totalTime*100 : 0);
    }

    LOG_INFO("Detection:");
    LOG_INFO("  Preprocess:  %.2f ms (%.1f%%)", detectionPreprocessTime, 
             totalTime > 0 ? detectionPreprocessTime/totalTime*100 : 0);
    LOG_INFO("  Inference:   %.2f ms (%.1f%%)", detectionInferenceTime,
             totalTime > 0 ? detectionInferenceTime/totalTime*100 : 0);
    LOG_INFO("  Postprocess: %.2f ms (%.1f%%)", detectionPostprocessTime,
             totalTime > 0 ? detectionPostprocessTime/totalTime*100 : 0);
    LOG_INFO("  Total:       %.2f ms (%.1f%%)", detectionTime,
             totalTime > 0 ? detectionTime/totalTime*100 : 0);
    
    LOG_INFO("Classification:");
    LOG_INFO("  Preprocess:  %.2f ms (%.1f%%)", classificationPreprocessTime,
             totalTime > 0 ? classificationPreprocessTime/totalTime*100 : 0);
    LOG_INFO("  Inference:   %.2f ms (%.1f%%)", classificationInferenceTime,
             totalTime > 0 ? classificationInferenceTime/totalTime*100 : 0);
    LOG_INFO("  Postprocess: %.2f ms (%.1f%%)", classificationPostprocessTime,
             totalTime > 0 ? classificationPostprocessTime/totalTime*100 : 0);
    LOG_INFO("  Total:       %.2f ms (%.1f%%)", classificationTime,
             totalTime > 0 ? classificationTime/totalTime*100 : 0);
    
    LOG_INFO("Recognition:");
    LOG_INFO("  Preprocess:  %.2f ms (%.1f%%)", recognitionPreprocessTime,
             totalTime > 0 ? recognitionPreprocessTime/totalTime*100 : 0);
    LOG_INFO("  Inference:   %.2f ms (%.1f%%)", recognitionInferenceTime,
             totalTime > 0 ? recognitionInferenceTime/totalTime*100 : 0);
    LOG_INFO("  Postprocess: %.2f ms (%.1f%%)", recognitionPostprocessTime,
             totalTime > 0 ? recognitionPostprocessTime/totalTime*100 : 0);
    LOG_INFO("  Total:       %.2f ms (%.1f%%)", recognitionTime,
             totalTime > 0 ? recognitionTime/totalTime*100 : 0);
    
    LOG_INFO("Total Time: %.2f ms", totalTime);
    LOG_INFO("Detected: %d | Rotated: %d | Recognized: %d (%.1f%%)", 
             detectedBoxes, rotatedBoxes, recognizedBoxes, recognitionRate);
    LOG_INFO("============================================");
}

// ==================== OCRPipeline ====================

OCRPipeline::OCRPipeline(const OCRPipelineConfig& config)
    : config_(config), initialized_(false) {
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
        LOG_INFO("Text Classifier enabled");
    } else {
        LOG_INFO("Text Classifier disabled");
    }
    
    // 初始化Recognizer
    recognizer_ = std::make_unique<TextRecognizer>(config_.recognizerConfig);
    if (!recognizer_->Initialize()) {
        LOG_ERROR("Failed to initialize TextRecognizer");
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("✅ OCR Pipeline initialized successfully!\n");
    
    return true;
}

bool OCRPipeline::process(const cv::Mat& image,
                         std::vector<PipelineOCRResult>& results,
                         OCRPipelineStats* stats) {
    if (!initialized_) {
        LOG_ERROR("OCRPipeline not initialized");
        return false;
    }
    
    if (image.empty()) {
        LOG_ERROR("Input image is empty");
        return false;
    }
    
    results.clear();
    
    auto start_total = std::chrono::high_resolution_clock::now();
    
    LOG_DEBUG("Starting OCR Pipeline: %dx%d", image.cols, image.rows);
    
    // Step 0-1: Document Preprocessing Pipeline (Orientation + UVDoc)
    cv::Mat processedImage = image;
    double doc_preproc_time = 0.0;
    
    if (config_.useDocPreprocessing && docPreprocessing_) {
        auto preprocResult = docPreprocessing_->Process(image);
        
        if (preprocResult.success && !preprocResult.processedImage.empty()) {
            processedImage = preprocResult.processedImage;
            doc_preproc_time = preprocResult.totalTime;
            LOG_DEBUG("Doc preprocess: orientation=%s, unwarp=%s, time=%.2fms",
                      preprocResult.orientationApplied ? "yes" : "no",
                      preprocResult.unwarpingApplied ? "yes" : "no",
                      preprocResult.totalTime);
        } else {
            LOG_WARN("Document preprocessing failed");
            processedImage = image;
        }
    } else {
        processedImage = image;
    }
    
    // Cache processed image for visualization
    lastProcessedImage_ = processedImage;
    
    // Step 2: Detection
    std::vector<DeepXOCR::TextBox> boxes = detector_->detect(processedImage);
    
    // Get detailed detection timings from detector
    double det_preprocess = 0.0, det_inference = 0.0, det_postprocess = 0.0;
    detector_->getLastTimings(det_preprocess, det_inference, det_postprocess);
    double det_time = det_preprocess + det_inference + det_postprocess;
    
    LOG_INFO("Detection: %zu boxes, %.2fms", boxes.size(), det_time);
    
    if (boxes.empty()) {
        LOG_WARN("No text detected");
        if (stats) {
            stats->detectionPreprocessTime = det_preprocess;
            stats->detectionInferenceTime = det_inference;
            stats->detectionPostprocessTime = det_postprocess;
            stats->detectionTime = det_time;
            stats->classificationTime = 0.0;
            stats->recognitionTime = 0.0;
            stats->totalTime = det_time;
            stats->detectedBoxes = 0;
            stats->rotatedBoxes = 0;
            stats->recognizedBoxes = 0;
            stats->recognitionRate = 0.0;
        }
        return true;
    }
    
    // Sort boxes (top to bottom, left to right) - matching Python's sorted_boxes
    auto start_sort = std::chrono::high_resolution_clock::now();

    // Python: sorted(dt_boxes, key=lambda x: (x[0][1], x[0][0]))
    // Sort by first point's (y, x)
    std::sort(boxes.begin(), boxes.end(), [](const DeepXOCR::TextBox& a, const DeepXOCR::TextBox& b) {
        if (std::abs(a.points[0].y - b.points[0].y) < 1.0f) {
            return a.points[0].x < b.points[0].x;
        }
        return a.points[0].y < b.points[0].y;
    });
    
    // Additional sorting refinement (matching Python's bubble sort logic)
    // For boxes on the same line (y difference < 10), sort by x coordinate
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
    
    auto end_sort = std::chrono::high_resolution_clock::now();
    double sort_time = std::chrono::duration<double, std::milli>(end_sort - start_sort).count();
    LOG_INFO("Boxes sorted by (y, x), time: %.2fms", sort_time);
    
    // Step 3: Crop text regions
    auto start_crop = std::chrono::high_resolution_clock::now();
    std::vector<cv::Mat> crops;
    std::vector<std::vector<cv::Point2f>> box_points_list;
    crops.reserve(boxes.size());
    box_points_list.reserve(boxes.size());
    
    for (size_t i = 0; i < boxes.size(); ++i) {
        // Convert TextBox points to vector<Point2f>
        std::vector<cv::Point2f> box_points(4);
        for (int j = 0; j < 4; ++j) {
            box_points[j] = boxes[i].points[j];
        }
        
        // Crop text region from processedImage (after UVDoc if applied)
        cv::Mat textImage = Geometry::getRotateCropImage(processedImage, box_points);
        if (textImage.empty()) {
            continue;
        }
        
        crops.push_back(textImage);
        box_points_list.push_back(box_points);
    }
    
    auto end_crop = std::chrono::high_resolution_clock::now();
    double crop_time = std::chrono::duration<double, std::milli>(end_crop - start_crop).count();
    LOG_INFO("Cropped %zu regions, time: %.2fms", crops.size(), crop_time);
    
    // Step 4: Classification (optional)
    auto start_cls = std::chrono::high_resolution_clock::now();
    int rotated_count = 0;
    double cls_preprocess = 0.0, cls_inference = 0.0, cls_postprocess = 0.0;
    
    if (config_.useClassification && classifier_) {
        auto cls_results = classifier_->ClassifyBatch(crops);
        
        // Get detailed classification timings
        classifier_->getLastTimings(cls_preprocess, cls_inference, cls_postprocess);
        
        for (size_t i = 0; i < crops.size() && i < cls_results.size(); ++i) {
            auto [label, confidence] = cls_results[i];
            
            // Rotate if needed (label=="180" and confidence > threshold)
            if (classifier_->NeedsRotation(label, confidence)) {
                cv::rotate(crops[i], crops[i], cv::ROTATE_180);
                rotated_count++;
            }
        }
    }
    
    auto end_cls = std::chrono::high_resolution_clock::now();
    double cls_time = std::chrono::duration<double, std::milli>(end_cls - start_cls).count();
    LOG_INFO("Classification: rotated %d/%zu, %.2fms", rotated_count, crops.size(), cls_time);
    
    // Step 5: Recognition
    // Track recognition statistics and timing
    int recognized_count = 0;
    int filtered_count = 0;
    int zero_conf_count = 0;
    double rec_preprocess = 0.0, rec_inference = 0.0, rec_postprocess = 0.0;
    
    // Reset timing before recognition loop (Recognize() will accumulate timing)
    recognizer_->resetTimings();
    
    for (size_t i = 0; i < crops.size(); ++i) {
        // Recognize text (timing accumulates inside Recognize() calls)
        auto [text, confidence] = recognizer_->Recognize(crops[i]);
        
        if (!text.empty()) {
            recognized_count++;
            LOG_DEBUG("  ✓ Crop %zu/%zu: text='%s', conf=%.4f", 
                      i, crops.size(), text.c_str(), confidence);
            PipelineOCRResult ocr_result;
            ocr_result.box = box_points_list[i];
            ocr_result.text = text;
            ocr_result.confidence = confidence;
            ocr_result.index = static_cast<int>(i);
            results.push_back(ocr_result);
        } else {
            filtered_count++;
            if (confidence == 0.0f) {
                zero_conf_count++;
                LOG_DEBUG("  ✗ Crop %zu/%zu: ZERO CONFIDENCE, size=%dx%d", 
                          i, crops.size(), crops[i].cols, crops[i].rows);
            } else {
                LOG_DEBUG("  ✗ Crop %zu/%zu: FILTERED (conf=%.4f < threshold)", i, crops.size(), confidence);
            }
        }
    }
    
    // Get accumulated recognition timing
    recognizer_->getLastTimings(rec_preprocess, rec_inference, rec_postprocess);
    double rec_time = rec_preprocess + rec_inference + rec_postprocess;
    
    LOG_INFO("Recognition: %zu/%zu boxes, %.2fms", results.size(), boxes.size(), rec_time);
    
    auto end_total = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::milli>(end_total - start_total).count();
    
    LOG_INFO("Pipeline complete: %.2fms total", total_time);
    
    // Step 5: Sort results
    if (config_.sortResults && !results.empty()) {
        sortOCRResults(results);
        
        // Update index after sorting
        for (size_t i = 0; i < results.size(); ++i) {
            results[i].index = static_cast<int>(i);
        }
    }
    
    // Step 6: Statistics
    if (stats) {
        stats->docPreprocessingTime = doc_preproc_time;
        
        // Detection 详细时间
        stats->detectionPreprocessTime = det_preprocess;
        stats->detectionInferenceTime = det_inference;
        stats->detectionPostprocessTime = det_postprocess;
        stats->detectionTime = det_time;
        
        // Classification 详细时间
        stats->classificationPreprocessTime = cls_preprocess;
        stats->classificationInferenceTime = cls_inference;
        stats->classificationPostprocessTime = cls_postprocess;
        stats->classificationTime = cls_time;
        
        // Recognition 详细时间 (暂时分不清楚，后续优化)
        stats->recognitionPreprocessTime = rec_preprocess;
        stats->recognitionInferenceTime = rec_inference;
        stats->recognitionPostprocessTime = rec_postprocess;
        stats->recognitionTime = rec_time;
        
        stats->totalTime = total_time;
        stats->detectedBoxes = static_cast<int>(boxes.size());
        stats->rotatedBoxes = rotated_count;
        stats->recognizedBoxes = static_cast<int>(results.size());
        stats->recognitionRate = boxes.empty() ? 0.0 : 
            (static_cast<double>(results.size()) / boxes.size() * 100.0);
    }
    
    // Print model usage statistics
    if (recognizer_) {
        recognizer_->PrintModelUsageStats();
    }
    
    return true;
}

bool OCRPipeline::processWithVisualization(const cv::Mat& image,
                                          std::vector<PipelineOCRResult>& results,
                                          cv::Mat& visualImage,
                                          OCRPipelineStats* stats) {
    // Process the image (this will cache the processed image)
    if (!process(image, results, stats)) {
        return false;
    }
    
    // Use the cached processed image for visualization
    cv::Mat imageForVisualization = lastProcessedImage_.empty() ? image : lastProcessedImage_;
    
    // Create visualization on the processed image (after UVDoc if applied)
    if (!results.empty()) {
        // Convert OCRResult to TextBox for visualization
        std::vector<TextBox> textBoxes;
        for (const auto& result : results) {
            TextBox box;
            for (size_t i = 0; i < 4 && i < result.box.size(); ++i) {
                box.points[i] = result.box[i];
            }
            box.text = result.text;
            box.confidence = result.confidence;
            textBoxes.push_back(box);
        }
        
        // Draw OCR results on processed image
        visualImage = Visualizer::drawOCRResultsSideBySide(imageForVisualization, textBoxes);
    } else {
        visualImage = image.clone();
    }
    
    return true;
}

int OCRPipeline::processBatch(const std::vector<cv::Mat>& images,
                             std::vector<std::vector<PipelineOCRResult>>& allResults,
                             OCRPipelineStats* stats) {
    if (!initialized_) {
        LOG_ERROR("OCRPipeline not initialized");
        return 0;
    }
    
    allResults.clear();
    allResults.resize(images.size());
    
    int successCount = 0;
    OCRPipelineStats totalStats;
    
    for (size_t i = 0; i < images.size(); ++i) {
        OCRPipelineStats singleStats;
        if (process(images[i], allResults[i], &singleStats)) {
            successCount++;
            
            // Accumulate statistics
            totalStats.detectionTime += singleStats.detectionTime;
            totalStats.recognitionTime += singleStats.recognitionTime;
            totalStats.totalTime += singleStats.totalTime;
            totalStats.detectedBoxes += singleStats.detectedBoxes;
            totalStats.recognizedBoxes += singleStats.recognizedBoxes;
        }
    }
    
    if (stats && successCount > 0) {
        *stats = totalStats;
        stats->recognitionRate = totalStats.detectedBoxes == 0 ? 0.0 :
            (static_cast<double>(totalStats.recognizedBoxes) / totalStats.detectedBoxes * 100.0);
    }
    
    return successCount;
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
    LOG_INFO("Results saved to: %s", jsonPath.c_str());
    
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

    // Initialize queues with a buffer size of 4
    detQueue_ = std::make_unique<ConcurrentQueue<DetectionTask>>(4);
    recQueue_ = std::make_unique<ConcurrentQueue<RecognitionTask>>(4);
    outQueue_ = std::make_unique<ConcurrentQueue<OutputTask>>(4);

    running_ = true;
    detThread_ = std::thread(&OCRPipeline::detectionLoop, this);
    recThread_ = std::thread(&OCRPipeline::recognitionLoop, this);
    
    LOG_INFO("Async pipeline started");
}

void OCRPipeline::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Push dummy tasks to unblock queues
    if (detQueue_) detQueue_->push({});
    if (recQueue_) recQueue_->push({});
    
    if (detThread_.joinable()) detThread_.join();
    if (recThread_.joinable()) recThread_.join();
    
    if (detQueue_) detQueue_->clear();
    if (recQueue_) recQueue_->clear();
    if (outQueue_) outQueue_->clear();
    
    LOG_INFO("Async pipeline stopped");
}

bool OCRPipeline::pushTask(const cv::Mat& image, int64_t id) {
    if (!running_ || !detQueue_) return false;
    detQueue_->push({image, id});
    return true;
}

bool OCRPipeline::getResult(std::vector<PipelineOCRResult>& results, int64_t& id) {
    if (!running_ || !outQueue_) return false;
    
    OutputTask task = outQueue_->pop();
    // Check if it's a valid task (e.g. during shutdown we might pop empty tasks)
    if (!running_) return false;
    
    results = std::move(task.results);
    id = task.id;
    return true;
}

void OCRPipeline::detectionLoop() {
    while (running_) {
        DetectionTask task = detQueue_->pop();
        if (!running_) break;
        if (task.image.empty()) continue;

        // 1. Doc Preprocessing
        cv::Mat processedImage = task.image;
        if (config_.useDocPreprocessing && docPreprocessing_) {
            auto preprocResult = docPreprocessing_->Process(task.image);
            if (preprocResult.success && !preprocResult.processedImage.empty()) {
                processedImage = preprocResult.processedImage;
            }
        }

        // 2. Detection
        std::vector<DeepXOCR::TextBox> boxes = detector_->detect(processedImage);

        // 3. Sort Boxes (Same logic as process())
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

        // Push to Recognition Queue
        if (recQueue_) {
            recQueue_->push({processedImage, std::move(boxes), task.id});
        }
    }
}

void OCRPipeline::recognitionLoop() {
    while (running_) {
        RecognitionTask task = recQueue_->pop();
        if (!running_) break;
        
        std::vector<PipelineOCRResult> results;
        results.reserve(task.boxes.size());

        // Prepare crops
        std::vector<cv::Mat> crops;
        std::vector<std::vector<cv::Point2f>> box_points_list;
        crops.reserve(task.boxes.size());
        box_points_list.reserve(task.boxes.size());

        for (size_t i = 0; i < task.boxes.size(); ++i) {
            std::vector<cv::Point2f> box_points(4);
            for (int j = 0; j < 4; ++j) box_points[j] = task.boxes[i].points[j];
            
            cv::Mat textImage = Geometry::getRotateCropImage(task.image, box_points);
            if (textImage.empty()) continue;
            
            crops.push_back(textImage);
            box_points_list.push_back(box_points);
        }

        // Classification
        if (config_.useClassification && classifier_) {
            auto cls_results = classifier_->ClassifyBatch(crops);
            for (size_t i = 0; i < crops.size() && i < cls_results.size(); ++i) {
                auto [label, confidence] = cls_results[i];
                if (classifier_->NeedsRotation(label, confidence)) {
                    cv::rotate(crops[i], crops[i], cv::ROTATE_180);
                }
            }
        }

        // Recognition
        for (size_t i = 0; i < crops.size(); ++i) {
            auto [text, confidence] = recognizer_->Recognize(crops[i]);
            
            if (!text.empty()) {
                 PipelineOCRResult res;
                 res.box = box_points_list[i];
                 res.text = text;
                 res.confidence = confidence;
                 res.index = static_cast<int>(results.size());
                 results.push_back(res);
            }
        }
        
        // Sort results
        if (config_.sortResults && !results.empty()) {
             sortOCRResults(results);
             for (size_t i = 0; i < results.size(); ++i) {
                 results[i].index = static_cast<int>(i);
             }
        }

        if (outQueue_) {
            outQueue_->push({std::move(results), task.id});
        }
    }
}

} // namespace ocr

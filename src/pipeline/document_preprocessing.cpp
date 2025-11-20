/**
 * @file document_preprocessing.cpp
 * @brief Document Preprocessing Pipeline Implementation
 */

#include "pipeline/document_preprocessing.h"
#include "common/logger.hpp"
#include <chrono>

namespace ocr {

// ==================== DocumentPreprocessingConfig ====================

void DocumentPreprocessingConfig::Show() const {
    LOG_INFO("========== Document Preprocessing Pipeline Config ==========");
    LOG_INFO("Use Orientation Correction: %s", useOrientation ? "true" : "false");
    if (useOrientation) {
        LOG_INFO("  Orientation Model: %s", orientationConfig.modelPath.c_str());
        LOG_INFO("  Confidence Threshold: %.3f", orientationConfig.confidenceThreshold);
    }
    
    LOG_INFO("Use Document Unwarping: %s", useUnwarping ? "true" : "false");
    if (useUnwarping) {
        LOG_INFO("  UVDoc Model: %s", uvdocConfig.modelPath.c_str());
        LOG_INFO("  Input Size: %dx%d", uvdocConfig.inputWidth, uvdocConfig.inputHeight);
        LOG_INFO("  Align Corners: %s", uvdocConfig.alignCorners ? "true" : "false");
    }
    LOG_INFO("===========================================================");
}

// ==================== DocumentPreprocessingPipeline ====================

DocumentPreprocessingPipeline::DocumentPreprocessingPipeline(const DocumentPreprocessingConfig& config)
    : config_(config), initialized_(false) {
}

DocumentPreprocessingPipeline::~DocumentPreprocessingPipeline() {
    LOG_INFO("[~DocumentPreprocessingPipeline] Pipeline destroyed");
}

bool DocumentPreprocessingPipeline::Initialize() {
    if (initialized_) {
        LOG_WARN("[Initialize] Pipeline already initialized");
        return true;
    }
    
    LOG_INFO("[Initialize] Initializing Document Preprocessing Pipeline...");
    
    // Stage 1: Initialize Orientation Classifier (optional)
    if (config_.useOrientation) {
        LOG_INFO("[Initialize] Loading Document Orientation Classifier...");
        orientationClassifier_ = std::make_unique<DocumentOrientationClassifier>(config_.orientationConfig);
        
        if (!orientationClassifier_->LoadModel()) {
            LOG_ERROR("[Initialize] Failed to load Document Orientation model");
            LOG_WARN("[Initialize] Continuing without orientation correction");
            orientationClassifier_ = nullptr;
            config_.useOrientation = false;
        } else {
            LOG_INFO("[Initialize] ✓ Document Orientation Classifier loaded");
        }
    } else {
        LOG_INFO("[Initialize] Document Orientation: DISABLED");
    }
    
    // Stage 2: Initialize UVDoc Processor (optional)
    if (config_.useUnwarping) {
        LOG_INFO("[Initialize] Loading UVDoc Document Unwarping Processor...");
        uvdocProcessor_ = std::make_unique<UVDocProcessor>(config_.uvdocConfig);
        
        if (!uvdocProcessor_->LoadModel()) {
            LOG_ERROR("[Initialize] Failed to load UVDoc model");
            LOG_WARN("[Initialize] Continuing without document unwarping");
            uvdocProcessor_ = nullptr;
            config_.useUnwarping = false;
        } else {
            LOG_INFO("[Initialize] ✓ UVDoc Processor loaded");
        }
    } else {
        LOG_INFO("[Initialize] Document Unwarping: DISABLED");
    }
    
    initialized_ = true;
    LOG_INFO("[Initialize] ✓ Document Preprocessing Pipeline initialized successfully");
    
    return true;
}

DocumentPreprocessingResult DocumentPreprocessingPipeline::Process(const cv::Mat& image) {
    DocumentPreprocessingResult result;
    result.success = false;
    
    if (!initialized_) {
        LOG_ERROR("[Process] Pipeline not initialized");
        return result;
    }
    
    if (image.empty()) {
        LOG_ERROR("[Process] Input image is empty");
        return result;
    }
    
    auto totalStart = std::chrono::high_resolution_clock::now();
    
    LOG_DEBUG("Doc preprocessing: %dx%d", image.cols, image.rows);
    
    cv::Mat currentImage = image.clone();
    
    // Stage 1: Document Orientation Correction
    currentImage = ProcessOrientation(currentImage, result);
    
    // Stage 2: Document Unwarping (UVDoc)
    currentImage = ProcessUnwarping(currentImage, result);
    
    auto totalEnd = std::chrono::high_resolution_clock::now();
    result.totalTime = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
    
    result.processedImage = currentImage;
    result.success = true;
    
    return result;
}

cv::Mat DocumentPreprocessingPipeline::ProcessOrientation(const cv::Mat& image, 
                                                          DocumentPreprocessingResult& result) {
    cv::Mat currentImage = image;
    
    if (!config_.useOrientation || !orientationClassifier_) {
        result.orientationApplied = false;
        result.orientationTime = 0.0f;
        return currentImage;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 检测文档方向
    auto orientationResult = orientationClassifier_->Classify(image);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.orientationTime = std::chrono::duration<float, std::milli>(end - start).count();
    
    result.detectedAngle = orientationResult.angle;
    result.orientationConfidence = orientationResult.confidence;
    
    LOG_DEBUG("Orientation: angle=%d°, conf=%.4f, time=%.2fms", 
              orientationResult.angle, orientationResult.confidence, result.orientationTime);
    
    // 应用旋转
    if (orientationResult.angle != 0) {
        currentImage = DocumentOrientationClassifier::RotateImage(image, orientationResult.angle);
        result.orientationApplied = true;
    } else {
        result.orientationApplied = false;
    }
    
    return currentImage;
}

cv::Mat DocumentPreprocessingPipeline::ProcessUnwarping(const cv::Mat& image, 
                                                        DocumentPreprocessingResult& result) {
    cv::Mat currentImage = image;
    
    if (!config_.useUnwarping || !uvdocProcessor_) {
        result.unwarpingApplied = false;
        result.unwarpingTime = 0.0f;
        return currentImage;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 执行文档畸变校正
    auto uvdocResult = uvdocProcessor_->Process(image);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.unwarpingTime = std::chrono::duration<float, std::milli>(end - start).count();
    
    if (uvdocResult.success && !uvdocResult.correctedImage.empty()) {
        currentImage = uvdocResult.correctedImage;
        result.unwarpingApplied = true;
        LOG_DEBUG("UVDoc unwarp: success, time=%.2fms", result.unwarpingTime);
    } else {
        result.unwarpingApplied = false;
        LOG_WARN("UVDoc unwarp failed");
        currentImage = image.clone();
    }
    
    return currentImage;
}

} // namespace ocr

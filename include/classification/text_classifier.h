#pragma once

#include <dxrt/dxrt_api.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>
#include <string>

#include "common/logger.hpp"
#include "common/types.hpp"

namespace ocr {

/**
 * Text Classifier Configuration
 * Based on PP-OCRv5 Classification for text orientation detection
 */
struct ClassifierConfig {
    // Model path (default - will be resolved to absolute path)
    std::string modelPath = std::string(PROJECT_ROOT_DIR) + "/engine/model_files/best/textline_ori.dxnn";
    
    // Classification threshold (rotate if score > threshold)
    float threshold = 0.9f;
    
    // Input size (fixed for classification model)
    int inputWidth = 160;
    int inputHeight = 80;
    
    // Mean and scale for normalization
    std::vector<float> mean = {0.485f, 0.456f, 0.406f};
    std::vector<float> scale = {0.229f, 0.224f, 0.225f};
    
    void Show() const {
        LOG_INFO("ClassifierConfig:");
        LOG_INFO("  modelPath=%s", modelPath.c_str());
        LOG_INFO("  threshold=%.2f", threshold);
        LOG_INFO("  inputSize=%dx%d", inputWidth, inputHeight);
    }
};

/**
 * Text Classifier Class
 * Detects if text image needs 180-degree rotation
 */
class TextClassifier {
public:
    TextClassifier() = default;
    explicit TextClassifier(const ClassifierConfig& config);
    ~TextClassifier() = default;
    
    // Initialize model
    bool Initialize();
    
    // Classify single text image
    // Returns: (label, confidence)
    //   label: "0" (normal) or "180" (needs rotation)
    //   confidence: probability [0, 1]
    std::pair<std::string, float> Classify(const cv::Mat& textImage);
    
    // Classify batch of text images
    std::vector<std::pair<std::string, float>> ClassifyBatch(
        const std::vector<cv::Mat>& textImages);
    
    // Check if image needs rotation based on classification result
    bool NeedsRotation(const std::string& label, float confidence) const {
        return (label == "180" && confidence > config_.threshold);
    }
    
private:
    ClassifierConfig config_;
    std::unique_ptr<dxrt::InferenceEngine> engine_;
    std::vector<std::string> labels_ = {"0", "180"};
    bool initialized_ = false;
    
    // Preprocessing
    cv::Mat Preprocess(const cv::Mat& image);
    
    // Postprocessing (argmax + softmax)
    std::pair<std::string, float> Postprocess(dxrt::TensorPtrs& outputs);
};

} // namespace ocr

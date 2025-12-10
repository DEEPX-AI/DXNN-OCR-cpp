#pragma once

#include <dxrt/dxrt_api.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <map>
#include <memory>

#include "common/logger.hpp"
#include "common/types.hpp"
#include "recognition/rec_postprocess.h"  // 包含完整定义

namespace DeepXOCR {

/**
 * Text Recognizer Configuration
 * Based on PP-OCRv5 CRNN architecture with multi-ratio models
 */
struct RecognizerConfig {
    // Recognition threshold
    float confThreshold = 0.3f;
    
    // Model paths for different aspect ratios (default paths - will be resolved to absolute paths)
    std::map<int, std::string> modelPaths = {
        {3, std::string(PROJECT_ROOT_DIR) + "/engine/model_files/server/rec_v5_ratio_3.dxnn"},
        {5, std::string(PROJECT_ROOT_DIR) + "/engine/model_files/server/rec_v5_ratio_5.dxnn"},
        {10, std::string(PROJECT_ROOT_DIR) + "/engine/model_files/server/rec_v5_ratio_10.dxnn"},
        {15, std::string(PROJECT_ROOT_DIR) + "/engine/model_files/server/rec_v5_ratio_15.dxnn"},
        {25, std::string(PROJECT_ROOT_DIR) + "/engine/model_files/server/rec_v5_ratio_25.dxnn"},
        {35, std::string(PROJECT_ROOT_DIR) + "/engine/model_files/server/rec_v5_ratio_35.dxnn"}
    };
    
    // Use mobile model variant (rec_mobile_* instead of rec_v5_*)
    bool useMobileModel = false;
    
    // Character dictionary path (default - will be resolved to absolute path)
    std::string dictPath = std::string(PROJECT_ROOT_DIR) + "/engine/model_files/ppocrv5_dict.txt";
    
    // Input height (fixed at 48)
    int inputHeight = 48;
    
    void Show() const {
        LOG_INFO("RecognizerConfig:");
        LOG_INFO("  confThreshold={:.2f}", confThreshold);
        LOG_INFO("  dictPath={}", dictPath);
        LOG_INFO("  Models: {} ratios", modelPaths.size());
    }
};

/**
 * Text Recognizer Class
 * Recognizes text content from cropped text images
 */
class TextRecognizer {
public:
    TextRecognizer() = default;
    explicit TextRecognizer(const RecognizerConfig& config);
    ~TextRecognizer() = default;
    
    // Initialize models and dictionary
    bool Initialize();
    
    // Synchronous recognition (single text)
    std::pair<std::string, float> Recognize(const cv::Mat& textImage);
    
    // Synchronous recognition (batch)
    std::vector<std::pair<std::string, float>> RecognizeBatch(
        const std::vector<cv::Mat>& textImages);
    
    // Asynchronous recognition
    int RecognizeAsync(const cv::Mat& textImage, void* userArg = nullptr);
    
    // Wait for async result
    std::pair<std::string, float> Wait(int jobId);
    
    // Register callback for async mode
    // Callback signature: void(text, confidence, userArg)
    void RegisterCallback(std::function<void(const std::string&, float, void*)> callback);
    
    // Print model usage statistics
    void PrintModelUsageStats() const;
    
    // Get last batch recognition timing details
    void getLastTimings(double& preprocess, double& inference, double& postprocess) const {
        preprocess = last_preprocess_time_;
        inference = last_inference_time_;
        postprocess = last_postprocess_time_;
    }
    
    // Reset timing counters (useful before starting a new batch)
    void resetTimings() {
        last_preprocess_time_ = 0.0;
        last_inference_time_ = 0.0;
        last_postprocess_time_ = 0.0;
    }
    
private:
    RecognizerConfig config_;
    
    // Recognition models for different aspect ratios
    // ratio_3, ratio_5, ratio_10, ratio_15, ratio_25, ratio_35
    std::map<int, std::unique_ptr<dxrt::InferenceEngine>> models_;
    
    // User callback for async mode
    std::function<void(const std::string&, float, void*)> userCallback_;
    
    // Internal callback handler
    int internalCallback(dxrt::TensorPtrs& outputs, void* userArg);
    
    // CTC Decoder
    std::unique_ptr<ocr::CTCDecoder> decoder_;
    
    // Model usage statistics
    mutable std::map<int, int> model_usage_;
    
    // Timing details of last batch recognition
    double last_preprocess_time_ = 0.0;
    double last_inference_time_ = 0.0;
    double last_postprocess_time_ = 0.0;
    
    // Select appropriate model based on image aspect ratio
    dxrt::InferenceEngine* SelectModel(const cv::Mat& image);
    int CalculateRatio(int width, int height);
    
    // Preprocessing
    cv::Mat Preprocess(const cv::Mat& image, int ratio);
    
    // Postprocessing (CTC decoding)
    std::pair<std::string, float> Postprocess(dxrt::TensorPtrs& outputs);
};

} // namespace DeepXOCR

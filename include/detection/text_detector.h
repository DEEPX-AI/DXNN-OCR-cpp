#pragma once

#include <dxrt/dxrt_api.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <map>
#include <memory>
#include <string>

#include "common/logger.hpp"
#include "common/types.hpp"

namespace ocr {

// Forward declaration
class DBPostProcessor;

/**
 * Text Detector Configuration
 * Based on PP-OCRv5 DBNet architecture
 */
struct DetectorConfig {
    // Detection thresholds
    float thresh = 0.3f;          // Binary threshold
    float boxThresh = 0.6f;       // Box confidence threshold
    float unclipRatio = 1.5f;     // Box expansion ratio
    int maxCandidates = 1500;     // Max number of candidate boxes
    
    // Model paths for different resolutions (default paths - will be resolved to absolute paths)
    std::string model640Path = std::string(PROJECT_ROOT_DIR) + "/engine/model_files/best/det_v5_640.dxnn";
    std::string model960Path = std::string(PROJECT_ROOT_DIR) + "/engine/model_files/best/det_v5_960.dxnn";
    
    // Image size threshold for model selection
    int sizeThreshold = 800;      // Use 640 if max(w,h) < threshold, else 960
    
    // Mean and scale for normalization
    std::vector<float> mean = {0.485f, 0.456f, 0.406f};
    std::vector<float> scale = {0.229f, 0.224f, 0.225f};
    
    // Pipeline visualization options
    bool saveIntermediates = false;
    std::string outputDir = "test";
    
    void Show() const;
};

/**
 * Text Detector Class
 * Detects text regions in images
 */
class TextDetector {
public:
    TextDetector() = default;
    explicit TextDetector(const DetectorConfig& config);
    ~TextDetector();
    
    /**
     * @brief Initialize detector with model files
     * @return true if successful
     */
    bool init();
    
    /**
     * @brief Detect text boxes in image
     * @param image Input image (BGR format)
     * @return Vector of detected text boxes
     */
    std::vector<DeepXOCR::TextBox> detect(const cv::Mat& image);

private:
    /**
     * @brief Select appropriate model based on image size
     */
    dxrt::InferenceEngine* selectModel(int height, int width);
    
    /**
     * @brief Preprocess image for detection
     */
    cv::Mat preprocess(const cv::Mat& image, int target_size, 
                      int& resized_h, int& resized_w);
    
    /**
     * @brief Run inference on preprocessed image
     */
    cv::Mat runInference(dxrt::InferenceEngine* engine, const cv::Mat& input);

private:
    DetectorConfig config_;
    std::unique_ptr<dxrt::InferenceEngine> model640_;
    std::unique_ptr<dxrt::InferenceEngine> model960_;
    std::unique_ptr<DBPostProcessor> postprocessor_;
    bool initialized_ = false;
};

} // namespace ocr

/**
 * @file uvdoc.h
 * @brief UVDoc Document Unwarping Module
 * 
 * This module implements document distortion correction using the UVDoc algorithm.
 * It corrects warped/curved document images to flat form by predicting UV displacement maps.
 * 
 * Pipeline:
 * 1. Resize image to fixed size (712x488)
 * 2. Run UVDoc model to get UV displacement map
 * 3. Use grid sampling to warp the original image
 * 
 * @author OCR Team
 * @date 2025-11-15
 */

#ifndef OCR_UVDOC_H
#define OCR_UVDOC_H

#include <opencv2/opencv.hpp>
#include <dxrt/dxrt_api.h>
#include <memory>
#include <string>

namespace ocr {

/**
 * @brief Configuration for UVDoc document unwarping
 */
struct UVDocConfig {
    std::string modelPath = std::string(PROJECT_ROOT_DIR) + "/engine/model_files/best/UVDoc_pruned_p3.dxnn";  ///< Path to UVDoc model file (default - absolute path)
    int inputWidth = 488;               ///< Model input width (Python: size=[712,488] -> width=488)
    int inputHeight = 712;              ///< Model input height (Python: size=[712,488] -> height=712)
    bool alignCorners = true;           ///< Use align_corners in grid sampling
    
    void Show() const;
};

/**
 * @brief Result of document unwarping
 */
struct UVDocResult {
    cv::Mat correctedImage;             ///< Unwarped/corrected image
    bool success = false;               ///< Whether correction was successful
    float inferenceTime = 0.0f;         ///< Inference time in milliseconds
};

/**
 * @brief UVDoc Document Unwarping Processor
 * 
 * Corrects warped/curved document images to flat form using deep learning-based
 * UV displacement map prediction and grid sampling.
 */
class UVDocProcessor {
public:
    /**
     * @brief Constructor
     * @param config UVDoc configuration
     */
    explicit UVDocProcessor(const UVDocConfig& config);
    
    /**
     * @brief Destructor
     */
    ~UVDocProcessor();
    
    /**
     * @brief Load UVDoc model
     * @return true if successful, false otherwise
     */
    bool LoadModel();
    
    /**
     * @brief Process image to correct document distortion
     * @param image Input image (warped document)
     * @return UVDocResult containing corrected image and metadata
     */
    UVDocResult Process(const cv::Mat& image);
    
private:
    /**
     * @brief Preprocess input image for model inference
     * @param image Input image
     * @return Preprocessed image ready for inference
     */
    cv::Mat Preprocess(const cv::Mat& image);
    
    /**
     * @brief Run inference to get UV displacement map
     * @param preprocessed Preprocessed image
     * @param uvMap Output UV displacement map [2, H, W]
     * @return Inference time in milliseconds
     */
    float Inference(const cv::Mat& preprocessed, cv::Mat& uvMap);
    
    /**
     * @brief Post-process UV map and apply grid sampling to correct image
     * @param uvMap UV displacement map from model [2, H, W]
     * @param originalImage Original input image
     * @return Corrected/unwarped image
     */
    cv::Mat Postprocess(const cv::Mat& uvMap, const cv::Mat& originalImage);
    
    /**
     * @brief Apply grid sampling using UV displacement map
     * @param image Input image
     * @param uvMap UV displacement map [2, H, W]
     * @param alignCorners Whether to use align_corners mode
     * @return Warped/corrected image
     */
    cv::Mat GridSample(const cv::Mat& image, const cv::Mat& uvMap, bool alignCorners);
    
    /**
     * @brief Resize with align_corners mode (PyTorch-style)
     * @param image Input image
     * @param targetSize Target size (width, height)
     * @return Resized image
     */
    cv::Mat ResizeAlignCorners(const cv::Mat& image, const cv::Size& targetSize);

private:
    UVDocConfig config_;
    dxrt::InferenceEngine* engine_ = nullptr;
    bool modelLoaded_ = false;
};

} // namespace ocr

#endif // OCR_UVDOC_H

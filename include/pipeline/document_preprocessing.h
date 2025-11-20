/**
 * @file document_preprocessing.h
 * @brief Document Preprocessing Pipeline - 统一的文档预处理管道
 * 
 * 完全对标 Python 的 DocumentPreprocessingPipeline 实现
 * 
 * Pipeline 顺序：
 * 1. Document Orientation Correction (文档方向校正) - 0°/90°/180°/270°
 * 2. Document Unwarping (文档畸变校正) - UVDoc
 * 
 * @author OCR Team
 * @date 2025-11-15
 */

#pragma once

#include "pipeline/document_orientation.h"
#include "preprocessing/uvdoc.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <string>

namespace ocr {

/**
 * @brief Document Preprocessing Pipeline 配置
 */
struct DocumentPreprocessingConfig {
    // Document Orientation 配置
    DocumentOrientationConfig orientationConfig;
    bool useOrientation = true;              // 是否使用文档方向校正
    
    // UVDoc 配置
    UVDocConfig uvdocConfig;
    bool useUnwarping = true;                // 是否使用文档畸变校正
    
    void Show() const;
};

/**
 * @brief Document Preprocessing Pipeline 处理结果
 */
struct DocumentPreprocessingResult {
    cv::Mat processedImage;                  // 处理后的图像
    bool success = false;                    // 处理是否成功
    
    // 详细信息
    int detectedAngle = 0;                   // 检测到的旋转角度
    float orientationConfidence = 0.0f;      // 方向检测置信度
    bool orientationApplied = false;         // 是否应用了方向校正
    bool unwarpingApplied = false;           // 是否应用了畸变校正
    
    // 性能统计
    float orientationTime = 0.0f;            // 方向校正耗时 (ms)
    float unwarpingTime = 0.0f;              // 畸变校正耗时 (ms)
    float totalTime = 0.0f;                  // 总耗时 (ms)
};

/**
 * @brief Document Preprocessing Pipeline
 * 
 * 完整的文档预处理流程，对标 Python 的 DocumentPreprocessingPipeline
 * 
 * 功能：
 * 1. Stage 1: Document Orientation (文档方向检测和校正)
 *    - 检测文档是否旋转 0°/90°/180°/270°
 *    - 自动旋转到正确方向
 * 
 * 2. Stage 2: Document Unwarping (文档畸变校正)
 *    - 使用 UVDoc 算法校正弯曲/畸变的文档
 *    - 输出平整的文档图像
 * 
 * 使用场景：
 * - 在 OCR Pipeline 的最开始阶段处理原始图像
 * - 提高后续 Detection 和 Recognition 的准确率
 */
class DocumentPreprocessingPipeline {
public:
    /**
     * @brief 构造函数
     * @param config Pipeline 配置
     */
    explicit DocumentPreprocessingPipeline(const DocumentPreprocessingConfig& config);
    
    /**
     * @brief 析构函数
     */
    ~DocumentPreprocessingPipeline();
    
    /**
     * @brief 初始化 Pipeline（加载模型）
     * @return true 表示成功，false 表示失败
     */
    bool Initialize();
    
    /**
     * @brief 处理图像（完整的预处理流程）
     * @param image 输入图像
     * @return 预处理结果（包含处理后的图像和统计信息）
     */
    DocumentPreprocessingResult Process(const cv::Mat& image);
    
    /**
     * @brief 仅执行 Stage 1: Orientation Correction
     * @param image 输入图像
     * @param result 输出结果
     * @return 校正后的图像
     */
    cv::Mat ProcessOrientation(const cv::Mat& image, DocumentPreprocessingResult& result);
    
    /**
     * @brief 仅执行 Stage 2: Document Unwarping
     * @param image 输入图像
     * @param result 输出结果
     * @return 校正后的图像
     */
    cv::Mat ProcessUnwarping(const cv::Mat& image, DocumentPreprocessingResult& result);
    
    /**
     * @brief 获取配置
     */
    const DocumentPreprocessingConfig& GetConfig() const { return config_; }
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return initialized_; }

private:
    DocumentPreprocessingConfig config_;
    bool initialized_ = false;
    
    // Stage 1: Document Orientation
    std::unique_ptr<DocumentOrientationClassifier> orientationClassifier_;
    
    // Stage 2: Document Unwarping (UVDoc)
    std::unique_ptr<UVDocProcessor> uvdocProcessor_;
};

} // namespace ocr

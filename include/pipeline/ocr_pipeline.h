#pragma once

#include "detection/text_detector.h"
#include "classification/text_classifier.h"
#include "recognition/text_recognizer.h"
#include "pipeline/document_preprocessing.h"
#include "common/types.hpp"
#include "common/visualizer.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

namespace ocr {

// TextDetector和DetectorConfig在ocr命名空间
// TextRecognizer在DeepXOCR命名空间
using DeepXOCR::TextRecognizer;
using DeepXOCR::RecognizerConfig;
using DeepXOCR::TextBox;

/**
 * @brief OCR Pipeline配置
 */
struct OCRPipelineConfig {
    // Detection配置
    DetectorConfig detectorConfig;
    
    // Document Preprocessing配置（统一管理 Orientation + UVDoc）
    DocumentPreprocessingConfig docPreprocessingConfig;
    bool useDocPreprocessing = true;  // 是否使用文档预处理
    
    // Classification配置
    ClassifierConfig classifierConfig;
    bool useClassification = true;    // 是否使用文本方向分类
    
    // Recognition配置
    RecognizerConfig recognizerConfig;
    
    // Pipeline配置
    bool enableVisualization = true;  // 是否生成可视化结果
    bool sortResults = true;          // 是否对结果排序（从上到下，从左到右）
    
    void Show() const;
};

/**
 * @brief OCR识别结果（单个文本框）
 */
struct PipelineOCRResult {
    std::vector<cv::Point2f> box;  // 文本框四个顶点坐标
    std::string text;               // 识别的文本内容
    float confidence;               // 置信度 [0, 1]
    int index;                      // 排序后的索引（从0开始）
    
    // 辅助方法：获取边界矩形
    cv::Rect getBoundingRect() const;
    
    // 辅助方法：获取中心点
    cv::Point2f getCenter() const;
};

/**
 * @brief OCR Pipeline性能统计
 */
struct OCRPipelineStats {
    double detectionTime = 0.0;        // Detection耗时 (ms)
    double classificationTime = 0.0;   // Classification耗时 (ms)
    double recognitionTime = 0.0;      // Recognition耗时 (ms)
    double totalTime = 0.0;            // 总耗时 (ms)
    
    int detectedBoxes = 0;             // 检测到的文本框数量
    int rotatedBoxes = 0;              // 旋转的文本框数量（180度）
    int recognizedBoxes = 0;           // 成功识别的文本框数量
    double recognitionRate = 0.0;      // 识别率 (%)
    
    void Show() const;
};

/**
 * @brief 完整的OCR Pipeline
 * 
 * 功能：
 * 1. 文本检测（Detection）
 * 2. 文本方向分类（Classification）
 * 3. 文本识别（Recognition）
 * 4. 结果排序（从上到下，从左到右）
 * 5. 可视化输出
 * 6. 性能统计
 */
class OCRPipeline {
public:
    /**
     * @brief 构造函数
     * @param config Pipeline配置
     */
    explicit OCRPipeline(const OCRPipelineConfig& config);
    
    /**
     * @brief 析构函数
     */
    ~OCRPipeline();
    
    /**
     * @brief 初始化Pipeline（加载模型）
     * @return true表示成功，false表示失败
     */
    bool initialize();
    
    /**
     * @brief 处理单张图片
     * @param image 输入图片
     * @param results 输出OCR结果
     * @param stats 输出性能统计（可选）
     * @return true表示成功，false表示失败
     */
    bool process(const cv::Mat& image, 
                std::vector<PipelineOCRResult>& results,
                OCRPipelineStats* stats = nullptr);
    
    /**
     * @brief 处理单张图片（带可视化）
     * @param image 输入图片
     * @param results 输出OCR结果
     * @param visualImage 输出可视化图片
     * @param stats 输出性能统计（可选）
     * @return true表示成功，false表示失败
     */
    bool processWithVisualization(const cv::Mat& image,
                                 std::vector<PipelineOCRResult>& results,
                                 cv::Mat& visualImage,
                                 OCRPipelineStats* stats = nullptr);
    
    /**
     * @brief 批量处理图片
     * @param images 输入图片列表
     * @param allResults 输出所有OCR结果
     * @param stats 输出整体性能统计（可选）
     * @return 成功处理的图片数量
     */
    int processBatch(const std::vector<cv::Mat>& images,
                    std::vector<std::vector<PipelineOCRResult>>& allResults,
                    OCRPipelineStats* stats = nullptr);
    
    /**
     * @brief 将结果保存为JSON
     * @param results OCR结果
     * @param jsonPath 输出JSON文件路径
     * @return true表示成功，false表示失败
     */
    static bool saveResultsToJSON(const std::vector<PipelineOCRResult>& results,
                                  const std::string& jsonPath);
    
    /**
     * @brief 获取最后处理的图片（经过文档预处理后）
     * @return 预处理后的图片，用于可视化时保证框坐标对齐
     */
    cv::Mat getLastProcessedImage() const { return lastProcessedImage_; }
    
private:
    /**
     * @brief 对OCR结果排序（从上到下，从左到右）
     * @param results OCR结果
     */
    void sortOCRResults(std::vector<PipelineOCRResult>& results);
    
    /**
     * @brief 比较两个OCR结果的位置（用于排序）
     * @param a 结果A
     * @param b 结果B
     * @return true表示a应该排在b前面
     */
    static bool compareOCRResults(const PipelineOCRResult& a, const PipelineOCRResult& b);
    
private:
    OCRPipelineConfig config_;
    std::unique_ptr<TextDetector> detector_;
    std::unique_ptr<DocumentPreprocessingPipeline> docPreprocessing_;
    std::unique_ptr<TextClassifier> classifier_;
    std::unique_ptr<TextRecognizer> recognizer_;
    bool initialized_ = false;
    
    // Cache the last processed image for visualization
    cv::Mat lastProcessedImage_;
};

} // namespace ocr

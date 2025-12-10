#pragma once

#include "detection/text_detector.h"
#include "classification/text_classifier.h"
#include "recognition/text_recognizer.h"
#include "pipeline/document_preprocessing.h"
#include "common/types.hpp"
#include "common/visualizer.h"
#include "common/concurrent_queue.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

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
 * @brief OCR Pipeline性能统计（详细版）
 */
struct OCRPipelineStats {
    // Document Preprocessing 阶段
    double docPreprocessingTime = 0.0;      // 文档预处理总时间 (ms)

    // Detection 阶段
    double detectionPreprocessTime = 0.0;   // 检测前处理 (ms)
    double detectionInferenceTime = 0.0;    // 检测推理 (ms)
    double detectionPostprocessTime = 0.0;  // 检测后处理 (ms)
    double detectionTime = 0.0;             // 检测总时间 (ms)
    
    // Classification 阶段
    double classificationPreprocessTime = 0.0;   // 分类前处理 (ms)
    double classificationInferenceTime = 0.0;    // 分类推理 (ms)
    double classificationPostprocessTime = 0.0;  // 分类后处理 (ms)
    double classificationTime = 0.0;             // 分类总时间 (ms)
    
    // Recognition 阶段
    double recognitionPreprocessTime = 0.0;   // 识别前处理 (ms)
    double recognitionInferenceTime = 0.0;    // 识别推理 (ms)
    double recognitionPostprocessTime = 0.0;  // 识别后处理 (ms)
    double recognitionTime = 0.0;             // 识别总时间 (ms)
    
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

    /**
     * @brief 启动异步处理线程
     */
    void start();

    /**
     * @brief 停止异步处理线程
     */
    void stop();

    /**
     * @brief 提交异步任务
     * @param image 输入图片
     * @param id 任务ID（用于匹配结果）
     * @return true表示提交成功（队列未满），false表示队列已满
     */
    bool pushTask(const cv::Mat& image, int64_t id);

    /**
     * @brief 获取异步结果
     * @param results 输出OCR结果
     * @param id 输出任务ID
     * @return true表示获取成功，false表示队列为空
     */
    bool getResult(std::vector<PipelineOCRResult>& results, int64_t& id, cv::Mat* processedImage = nullptr);
    
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

    // 异步处理相关定义
    struct DetectionTask {
        cv::Mat image;
        int64_t id;
    };

    struct RecognitionTask {
        cv::Mat image; // 预处理后的图片
        std::vector<TextBox> boxes;
        int64_t id;
    };

    struct OutputTask {
        std::vector<PipelineOCRResult> results;
        cv::Mat processedImage;  // UVDoc 处理后的图像（用于可视化）
        int64_t id;
    };

    // Context for tracking async recognition of an entire image
    struct RecognitionTaskContext {
        int64_t taskId;
        cv::Mat processedImage;                            // UVDoc 处理后的图像（用于可视化）
        std::vector<cv::Mat> crops;                        // Cropped images (keep alive during async)
        std::vector<std::vector<cv::Point2f>> boxPoints;  // Box coordinates for each crop
        std::vector<PipelineOCRResult> results;            // Results (one per crop)
        std::atomic<int> pendingCount{0};                  // Number of pending recognitions
        std::mutex resultMutex;                            // Protect results vector
        
        RecognitionTaskContext(int64_t id, size_t cropCount)
            : taskId(id), crops(cropCount), boxPoints(cropCount), results(cropCount) {
            pendingCount.store(static_cast<int>(cropCount));
        }
    };

    // Context for a single crop's async recognition
    struct RecognitionCropContext {
        std::shared_ptr<RecognitionTaskContext> taskCtx;
        size_t cropIndex;
    };

    void detectionLoop();
    void recognitionLoop();
    void onRecognitionComplete(const std::string& text, float confidence, void* userArg);

    std::unique_ptr<ConcurrentQueue<DetectionTask>> detQueue_;
    std::unique_ptr<ConcurrentQueue<RecognitionTask>> recQueue_;
    std::unique_ptr<ConcurrentQueue<OutputTask>> outQueue_;

    std::vector<std::thread> detThreads_;  // Multiple detection threads
    std::vector<std::thread> recThreads_;  // Multiple recognition threads
    std::atomic<bool> running_{false};
    
    int numDetectionThreads_;   // Set based on CPU cores
    int numRecognitionThreads_; // Set based on CPU cores
    
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

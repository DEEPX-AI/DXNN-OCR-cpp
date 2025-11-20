#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <vector>
#include <cmath>

namespace ocr {

/**
 * @brief Document Orientation分类配置
 */
struct DocumentOrientationConfig {
    std::string modelPath = std::string(PROJECT_ROOT_DIR) + "/engine/model_files/best/doc_ori_fixed.dxnn";  // doc_ori_fixed.dxnn 模型路径（默认 - 绝对路径）
    float confidenceThreshold = 0.9f;   // 置信度阈值（低于则默认为0°）- 与Python保持一致
    int inputHeight = 224;              // 输入高度
    int inputWidth = 224;               // 输入宽度
};

/**
 * @brief Document Orientation分类结果
 */
struct DocumentOrientationResult {
    int angle;                  // 预测的旋转角度 (0, 90, 180, 270)
    float confidence;           // 置信度 [0, 1]
    
    DocumentOrientationResult() : angle(0), confidence(0.0f) {}
    DocumentOrientationResult(int a, float c) : angle(a), confidence(c) {}
};

/**
 * @brief Document Orientation分类器
 * 
 * 功能：检测文档是否旋转了 0°/90°/180°/270°
 * 
 * 工作流程：
 * 1. 预处理：短边缩放到256 → 中心裁剪224×224 → 归一化
 * 2. 推理：使用DXRT运行doc_ori_fixed.dxnn模型
 * 3. 后处理：Softmax转换 → 选择最高概率 → 返回角度和置信度
 * 4. 图像旋转：根据预测角度旋转图像
 */
class DocumentOrientationClassifier {
public:
    /**
     * @brief 构造函数
     * @param config 配置参数
     */
    explicit DocumentOrientationClassifier(const DocumentOrientationConfig& config);
    
    /**
     * @brief 析构函数
     */
    ~DocumentOrientationClassifier();
    
    /**
     * @brief 加载模型
     * @return 加载成功返回true
     */
    bool LoadModel();
    
    /**
     * @brief 预处理：短边缩放 + 中心裁剪 + 归一化 + 转置
     * @param image 输入图像 (OpenCV Mat, HWC format, uint8)
     * @return 预处理后的数据 (CHW format, float)
     */
    std::vector<float> Preprocess(const cv::Mat& image);
    
    /**
     * @brief 推理：运行DXRT模型
     * @param preprocessed 预处理后的数据
     * @return 模型输出logits (4个值对应 0°/90°/180°/270°)
     */
    std::vector<float> Inference(const std::vector<float>& preprocessed);
    
    /**
     * @brief 后处理：Softmax + 选择最高概率
     * @param logits 模型输出logits
     * @return 分类结果（角度和置信度）
     */
    DocumentOrientationResult Postprocess(const std::vector<float>& logits);
    
    /**
     * @brief 完整的分类流程（预处理 → 推理 → 后处理）
     * @param image 输入图像
     * @return 分类结果
     */
    DocumentOrientationResult Classify(const cv::Mat& image);
    
    /**
     * @brief 根据预测的角度旋转图像
     * @param image 原始图像
     * @param angle 旋转角度 (0, 90, 180, 270)
     * @return 旋转后的图像
     */
    static cv::Mat RotateImage(const cv::Mat& image, int angle);
    
    /**
     * @brief 设置是否已初始化的标志
     */
    bool IsInitialized() const { return initialized_; }

private:
    DocumentOrientationConfig config_;
    bool initialized_ = false;
    void* model_handle_ = nullptr;  // DXRT模型句柄
    
    /**
     * @brief 短边缩放 (Resize with aspect ratio preserved)
     * @param image 输入图像
     * @param targetSize 目标短边长度
     * @return 缩放后的图像
     */
    cv::Mat ResizeShortSide(const cv::Mat& image, int targetSize);
    
    /**
     * @brief 中心裁剪
     * @param image 输入图像
     * @param cropSize 裁剪尺寸
     * @return 裁剪后的图像
     */
    cv::Mat CenterCrop(const cv::Mat& image, int cropSize);
    
    /**
     * @brief Softmax转换 (with numerical stability)
     * @param logits 输入logits
     * @return softmax概率
     */
    std::vector<float> Softmax(const std::vector<float>& logits);
    
    /**
     * @brief 图像归一化 (ImageNet标准)
     * @param image 输入图像 (float, [0, 1])
     * @return 归一化后的图像
     */
    cv::Mat Normalize(const cv::Mat& image);
};

} // namespace ocr

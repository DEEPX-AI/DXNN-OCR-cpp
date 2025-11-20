#include "pipeline/document_orientation.h"
#include "common/logger.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

// DXRT推理引擎头文件
#include <dxrt/dxrt_api.h>

namespace ocr {

DocumentOrientationClassifier::DocumentOrientationClassifier(const DocumentOrientationConfig& config)
    : config_(config) {
    LOG_INFO("DocumentOrientationClassifier created");
}

DocumentOrientationClassifier::~DocumentOrientationClassifier() {
    if (model_handle_ != nullptr) {
        auto* engine = static_cast<dxrt::InferenceEngine*>(model_handle_);
        delete engine;
        model_handle_ = nullptr;
    }
    LOG_INFO("DocumentOrientationClassifier destroyed");
}

bool DocumentOrientationClassifier::LoadModel() {
    if (config_.modelPath.empty()) {
        LOG_ERROR("Model path is empty");
        return false;
    }
    
    LOG_INFO("Loading doc_ori_fixed model from: %s", config_.modelPath.c_str());
    
    // 使用DXRT加载模型
    try {
        model_handle_ = new dxrt::InferenceEngine(config_.modelPath);
        LOG_INFO("doc_ori_fixed model loaded successfully");
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load model: %s", e.what());
        return false;
    }
}

cv::Mat DocumentOrientationClassifier::ResizeShortSide(const cv::Mat& image, int targetSize) {
    int height = image.rows;
    int width = image.cols;
    
    // 计算缩放比例（短边缩放到targetSize）
    double scale = static_cast<double>(targetSize) / std::min(height, width);
    
    int newHeight = static_cast<int>(height * scale);
    int newWidth = static_cast<int>(width * scale);
    
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);
    
    return resized;
}

cv::Mat DocumentOrientationClassifier::CenterCrop(const cv::Mat& image, int cropSize) {
    int height = image.rows;
    int width = image.cols;
    
    if (height < cropSize || width < cropSize) {
        LOG_WARN("Image size (%d×%d) smaller than crop size (%d×%d)", 
                 width, height, cropSize, cropSize);
        return image.clone();
    }
    
    // 计算中心裁剪的起点
    int x = (width - cropSize) / 2;
    int y = (height - cropSize) / 2;
    
    return image(cv::Rect(x, y, cropSize, cropSize)).clone();
}

cv::Mat DocumentOrientationClassifier::Normalize(const cv::Mat& image) {
    // ImageNet标准化参数
    const float mean[] = {0.485f, 0.456f, 0.406f};
    const float std[] = {0.229f, 0.224f, 0.225f};
    
    cv::Mat normalized;
    image.convertTo(normalized, CV_32F, 1.0 / 255.0);  // [0, 1]
    
    // 分离通道
    std::vector<cv::Mat> channels(3);
    cv::split(normalized, channels);
    
    // 对每个通道进行归一化
    for (int i = 0; i < 3; i++) {
        channels[i] = (channels[i] - mean[i]) / std[i];
    }
    
    // 合并通道
    cv::merge(channels, normalized);
    
    return normalized;
}

std::vector<float> DocumentOrientationClassifier::Softmax(const std::vector<float>& logits) {
    if (logits.size() != 4) {
        LOG_ERROR("Logits size should be 4, got %zu", logits.size());
        return std::vector<float>(4, 0.0f);
    }
    
    // 数值稳定性：减去最大值
    float max_logit = *std::max_element(logits.begin(), logits.end());
    
    std::vector<float> exp_values(4);
    float sum = 0.0f;
    
    for (int i = 0; i < 4; i++) {
        exp_values[i] = std::exp(logits[i] - max_logit);
        sum += exp_values[i];
    }
    
    // 归一化
    std::vector<float> probabilities(4);
    for (int i = 0; i < 4; i++) {
        probabilities[i] = exp_values[i] / sum;
    }
    
    return probabilities;
}

std::vector<float> DocumentOrientationClassifier::Preprocess(const cv::Mat& image) {
    if (image.empty()) {
        LOG_ERROR("Input image is empty");
        return std::vector<float>();
    }
    
    LOG_DEBUG("Preprocessing doc_ori image: %d×%d", image.cols, image.rows);
    
    // 1. 短边缩放到256
    cv::Mat resized = ResizeShortSide(image, 256);
    LOG_DEBUG("After resize: %d×%d", resized.cols, resized.rows);
    
    // 2. 中心裁剪224×224
    cv::Mat cropped = CenterCrop(resized, 224);
    
    // 3. 归一化 (ImageNet标准)
    cv::Mat normalized = Normalize(cropped);
    
    // 4. 转置 HWC → CHW
    std::vector<cv::Mat> channels(3);
    cv::split(normalized, channels);
    
    // 将3个224×224的通道展平为向量 (CHW格式)
    std::vector<float> result;
    result.reserve(3 * 224 * 224);
    
    for (int c = 0; c < 3; c++) {
        const float* ptr = channels[c].ptr<float>();
        for (int i = 0; i < 224 * 224; i++) {
            result.push_back(ptr[i]);
        }
    }
    
    LOG_DEBUG("Preprocessing complete: %zu elements", result.size());
    return result;
}

std::vector<float> DocumentOrientationClassifier::Inference(const std::vector<float>& preprocessed) {
    if (!initialized_) {
        LOG_ERROR("Model not initialized");
        return std::vector<float>(4, 0.0f);
    }
    
    if (preprocessed.size() != 3 * 224 * 224) {
        LOG_ERROR("Input size mismatch: expected %d, got %zu", 
                  3 * 224 * 224, preprocessed.size());
        return std::vector<float>(4, 0.0f);
    }
    
    LOG_DEBUG("Running doc_ori inference");
    
    // 使用DXRT推理
    auto* engine = static_cast<dxrt::InferenceEngine*>(model_handle_);
    if (!engine) {
        LOG_ERROR("Engine handle is null");
        return std::vector<float>(4, 0.0f);
    }
    
    try {
        // 准备输入数据（float格式，CHW）
        std::vector<uint8_t> uint8_input(3 * 224 * 224);
        for (size_t i = 0; i < preprocessed.size(); i++) {
            // 如果已经是float，可能需要转换或直接使用
            // 这取决于模型的具体要求
            uint8_input[i] = static_cast<uint8_t>(preprocessed[i]);
        }
        
        // 运行推理
        auto outputs = engine->Run(uint8_input.data());
        
        if (outputs.empty()) {
            LOG_ERROR("No output from inference");
            return std::vector<float>(4, 0.0f);
        }
        
        // 提取输出（第一个输出张量包含4个类的logits）
        auto* output_data = reinterpret_cast<const float*>(outputs[0]->data());
        
        // 获取输出张量的形状
        auto shape = outputs[0]->shape();
        size_t output_size = 1;
        for (auto dim : shape) {
            output_size *= dim;
        }
        
        if (output_size < 4) {
            LOG_ERROR("Output size too small: %zu", output_size);
            return std::vector<float>(4, 0.0f);
        }
        
        std::vector<float> logits(4);
        for (int i = 0; i < 4; i++) {
            logits[i] = output_data[i];
        }
        
        return logits;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Inference failed: %s", e.what());
        return std::vector<float>(4, 0.0f);
    }
}

DocumentOrientationResult DocumentOrientationClassifier::Postprocess(const std::vector<float>& logits) {
    if (logits.size() != 4) {
        LOG_ERROR("Logits size should be 4, got %zu", logits.size());
        return DocumentOrientationResult(0, 0.0f);
    }
    
    // Softmax转换
    std::vector<float> probabilities = Softmax(logits);
    
    // 找到最高概率的索引
    int max_idx = std::max_element(probabilities.begin(), probabilities.end()) 
                  - probabilities.begin();
    float max_prob = probabilities[max_idx];
    
    LOG_DEBUG("doc_ori probabilities: [0°=%.3f, 90°=%.3f, 180°=%.3f, 270°=%.3f]",
              probabilities[0], probabilities[1], probabilities[2], probabilities[3]);
    
    // 置信度阈值检查
    if (max_prob < config_.confidenceThreshold) {
        LOG_INFO("doc_ori confidence %.3f < threshold %.3f, defaulting to 0°",
                 max_prob, config_.confidenceThreshold);
        return DocumentOrientationResult(0, 1.0f);
    }
    
    // 映射索引到角度
    int angles[] = {0, 90, 180, 270};
    int angle = angles[max_idx];
    
    LOG_INFO("doc_ori detected angle: %d° (confidence: %.3f)", angle, max_prob);
    
    return DocumentOrientationResult(angle, max_prob);
}

DocumentOrientationResult DocumentOrientationClassifier::Classify(const cv::Mat& image) {
    auto preprocessed = Preprocess(image);
    auto logits = Inference(preprocessed);
    return Postprocess(logits);
}

cv::Mat DocumentOrientationClassifier::RotateImage(const cv::Mat& image, int angle) {
    cv::Mat rotated;
    
    switch (angle) {
        case 0:
            rotated = image.clone();
            break;
        case 90:
            cv::rotate(image, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        case 180:
            cv::rotate(image, rotated, cv::ROTATE_180);
            break;
        case 270:
            cv::rotate(image, rotated, cv::ROTATE_90_CLOCKWISE);
            break;
        default:
            LOG_WARN("Unknown rotation angle: %d", angle);
            rotated = image.clone();
            break;
    }
    
    return rotated;
}

} // namespace ocr

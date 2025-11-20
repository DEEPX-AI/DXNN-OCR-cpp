#include "classification/text_classifier.h"
#include "preprocessing/image_ops.h"
#include "common/logger.hpp"
#include <algorithm>
#include <cmath>

namespace ocr {

TextClassifier::TextClassifier(const ClassifierConfig& config)
    : config_(config) {
}

bool TextClassifier::Initialize() {
    if (config_.modelPath.empty()) {
        LOG_ERROR("Classification model path is empty");
        return false;
    }
    
    LOG_INFO("Loading classification model: %s", config_.modelPath.c_str());
    
    try {
        engine_ = std::make_unique<dxrt::InferenceEngine>(config_.modelPath);
        LOG_INFO("Classification model loaded successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load classification model: %s", e.what());
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("TextClassifier initialized successfully");
    
    return true;
}

std::pair<std::string, float> TextClassifier::Classify(const cv::Mat& textImage) {
    if (!initialized_) {
        LOG_ERROR("TextClassifier not initialized");
        return {"0", 0.0f};
    }
    
    if (textImage.empty()) {
        LOG_ERROR("Input image is empty");
        return {"0", 0.0f};
    }
    
    // 预处理
    cv::Mat preprocessed = Preprocess(textImage);
    if (preprocessed.empty()) {
        LOG_ERROR("Preprocessing failed");
        return {"0", 0.0f};
    }
    
    // 推理
    auto outputs = engine_->Run(preprocessed.data);
    if (outputs.empty()) {
        LOG_ERROR("Inference failed: no output tensors");
        return {"0", 0.0f};
    }
    
    // 后处理
    auto result = Postprocess(outputs);
    
    LOG_DEBUG("Classification result: label=%s, confidence=%.3f", 
              result.first.c_str(), result.second);
    
    return result;
}

std::vector<std::pair<std::string, float>> TextClassifier::ClassifyBatch(
    const std::vector<cv::Mat>& textImages) {
    
    std::vector<std::pair<std::string, float>> results;
    results.reserve(textImages.size());
    
    for (const auto& image : textImages) {
        results.push_back(Classify(image));
    }
    
    return results;
}

cv::Mat TextClassifier::Preprocess(const cv::Mat& image) {
    if (image.empty()) {
        LOG_ERROR("Input image is empty");
        return cv::Mat();
    }
    
    // Step 1: Resize to fixed size [80, 160]
    // Note: height=80, width=160
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(config_.inputWidth, config_.inputHeight));
    
    // Step 2: Convert to float and normalize
    // DXRT expects uint8 HWC format, so we DON'T need manual normalization
    // The normalization is baked into the model
    
    // Ensure the image is in the correct format (BGR, uint8, HWC)
    cv::Mat result;
    if (resized.type() != CV_8UC3) {
        resized.convertTo(result, CV_8UC3);
    } else {
        result = resized;
    }
    
    // Ensure contiguous memory
    if (!result.isContinuous()) {
        result = result.clone();
    }
    
    return result;
}

std::pair<std::string, float> TextClassifier::Postprocess(dxrt::TensorPtrs& outputs) {
    if (outputs.empty()) {
        LOG_ERROR("No output tensors");
        return {"0", 0.0f};
    }
    
    auto& output = outputs[0];
    
    // Get output shape
    auto shape = output->shape();
    if (shape.size() < 2) {
        LOG_ERROR("Invalid output shape dimension: %zu", shape.size());
        return {"0", 0.0f};
    }
    
    // Expected shape: [1, 2] or [2]
    size_t num_classes = shape[shape.size() - 1];
    if (num_classes != 2) {
        LOG_ERROR("Invalid number of classes: %zu (expected 2)", num_classes);
        return {"0", 0.0f};
    }
    
    // Get output data
    const float* data = reinterpret_cast<const float*>(output->data());
    if (!data) {
        LOG_ERROR("Failed to get output data");
        return {"0", 0.0f};
    }
    
    // Debug: Print raw outputs
    LOG_DEBUG("Raw outputs: [%.6f, %.6f]", data[0], data[1]);
    
    // Find argmax
    int max_idx = 0;
    float max_val = data[0];
    for (size_t i = 1; i < num_classes; ++i) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = static_cast<int>(i);
        }
    }
    
    // IMPORTANT: Model output is already Softmax-ed probabilities
    // Do NOT apply Softmax again!
    // Just use the max value directly as confidence
    float confidence = data[max_idx];
    std::string label = labels_[max_idx];
    
    LOG_DEBUG("Result: max_idx=%d, label='%s', confidence=%.6f", 
             max_idx, label.c_str(), confidence);
    
    return {label, confidence};
}

} // namespace ocr

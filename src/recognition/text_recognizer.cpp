#include "recognition/text_recognizer.h"
#include "recognition/rec_postprocess.h"
#include "preprocessing/image_ops.h"
#include "common/logger.hpp"
#include <algorithm>

namespace DeepXOCR {

TextRecognizer::TextRecognizer(const RecognizerConfig& config)
    : config_(config) {
}

bool TextRecognizer::Initialize() {
    if (config_.modelPaths.empty()) {
        LOG_ERROR("No recognition models specified");
        return false;
    }
    
    // 加载所有ratio模型
    LOG_INFO("Loading recognition models...");
    for (const auto& [ratio, model_path] : config_.modelPaths) {
        try {
            auto model = std::make_unique<dxrt::InferenceEngine>(model_path);
            models_[ratio] = std::move(model);
            LOG_INFO("  Loaded ratio_%d model: %s", ratio, model_path.c_str());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to load ratio_%d model: %s", ratio, e.what());
            return false;
        }
    }
    
    if (models_.empty()) {
        LOG_ERROR("No recognition models loaded");
        return false;
    }
    
    // 加载字符字典
    LOG_INFO("Loading character dictionary from: %s", config_.dictPath.c_str());
    decoder_ = std::make_unique<ocr::CTCDecoder>(config_.dictPath, true);
    
    if (decoder_->getDictSize() == 0) {
        LOG_ERROR("Failed to load character dictionary");
        return false;
    }
    
    LOG_INFO("TextRecognizer initialized successfully");
    LOG_INFO("  Models: %zu ratios", models_.size());
    LOG_INFO("  Dictionary: %zu characters", decoder_->getDictSize());
    
    return true;
}

std::pair<std::string, float> TextRecognizer::Recognize(const cv::Mat& textImage) {
    if (textImage.empty()) {
        LOG_ERROR("Input image is empty");
        return {"", 0.0f};
    }
    
    // 选择合适的模型
    auto* engine = SelectModel(textImage);
    if (!engine) {
        LOG_ERROR("No suitable model for image size %dx%d", 
                  textImage.cols, textImage.rows);
        return {"", 0.0f};
    }
    
    // 获取ratio
    int ratio = CalculateRatio(textImage.cols, textImage.rows);
    
    LOG_DEBUG("Recognize: input size=%dx%d, calculated ratio=%d, using model=ratio_%d",
              textImage.cols, textImage.rows, 
              textImage.cols / (textImage.rows > 0 ? textImage.rows : 1), ratio);
    
    // 预处理
    cv::Mat preprocessed = Preprocess(textImage, ratio);
    if (preprocessed.empty()) {
        LOG_ERROR("Preprocessing failed");
        return {"", 0.0f};
    }
    
    LOG_DEBUG("Preprocessed: size=%dx%d, type=%d, depth=%d, channels=%d, elemSize=%zu",
              preprocessed.cols, preprocessed.rows, preprocessed.type(), 
              preprocessed.depth(), preprocessed.channels(), preprocessed.elemSize());
    
    // Debug: check if data is valid
    if (preprocessed.data == nullptr) {
        LOG_ERROR("Preprocessed data is nullptr!");
        return {"", 0.0f};
    }
    
    // 推理
    auto outputs = engine->Run(preprocessed.data);
    if (outputs.empty()) {
        LOG_ERROR("Inference failed: no output tensors");
        return {"", 0.0f};
    }
    
    // 后处理 (CTC解码)
    auto result = Postprocess(outputs);
    
    // 置信度过滤
    if (result.second < config_.confThreshold) {
        LOG_DEBUG("Low confidence FILTERED: text='%s', conf=%.4f < threshold=%.4f", 
                  result.first.c_str(), result.second, config_.confThreshold);
        return {"", result.second};
    }
    
    return result;
}

std::vector<std::pair<std::string, float>> TextRecognizer::RecognizeBatch(
    const std::vector<cv::Mat>& textImages) {
    
    std::vector<std::pair<std::string, float>> results;
    results.reserve(textImages.size());
    
    for (const auto& image : textImages) {
        results.push_back(Recognize(image));
    }
    
    return results;
}

dxrt::InferenceEngine* TextRecognizer::SelectModel(const cv::Mat& image) {
    int ratio = CalculateRatio(image.cols, image.rows);
    
    // Track model usage statistics
    model_usage_[ratio]++;
    
    auto it = models_.find(ratio);
    if (it != models_.end()) {
        return it->second.get();
    }
    
    // 如果找不到精确匹配，使用最接近的ratio
    LOG_WARN("No exact model for ratio %d, using closest match", ratio);
    
    int closest_ratio = -1;
    int min_diff = INT_MAX;
    
    for (const auto& [r, _] : models_) {
        int diff = std::abs(r - ratio);
        if (diff < min_diff) {
            min_diff = diff;
            closest_ratio = r;
        }
    }
    
    if (closest_ratio != -1) {
        LOG_DEBUG("Using ratio_%d model instead", closest_ratio);
        model_usage_[closest_ratio]++;
        return models_[closest_ratio].get();
    }
    
    return nullptr;
}

int TextRecognizer::CalculateRatio(int width, int height) {
    if (height == 0) {
        return 35;  // 默认最大ratio
    }
    
    float ratio = static_cast<float>(width) / height;
    
    // 根据Python实现的逻辑
    if (ratio <= 3.0f) return 3;
    if (ratio <= 5.0f) return 5;
    if (ratio <= 10.0f) return 10;
    if (ratio <= 15.0f) return 15;
    if (ratio <= 25.0f) return 25;
    return 35;
}

cv::Mat TextRecognizer::Preprocess(const cv::Mat& image, int ratio) {
    // Recognition预处理：
    // 1. 固定高度48
    // 2. 宽度根据ratio计算（注意：ratio_3的宽度是120，不是144！）
    // 3. PPOCR方式：Pad → Resize
    
    int target_height = config_.inputHeight;  // 48
    int target_width;
    
    // Python的映射：ratio_3 -> 120, ratio_5 -> 240, ratio_10 -> 480, ...
    // ratio_rec_preprocess[0]['resize']['size'] = [48, 120] for ratio_3
    if (ratio == 3) {
        target_width = 120;  // Special case: 120 instead of 144
    } else {
        target_width = target_height * ratio;
    }
    
    LOG_DEBUG("Preprocessing: %dx%d -> %dx%d (ratio_%d)",
              image.cols, image.rows, target_width, target_height, ratio);
    
    // 使用PPOCR预处理方式（与Detection一致）
    int orig_h = image.rows;
    int orig_w = image.cols;
    
    float target_ratio = static_cast<float>(target_width) / target_height;
    float orig_ratio = static_cast<float>(orig_w) / orig_h;
    
    cv::Mat padded;
    
    // Step 1: Pad到目标ratio
    // IMPORTANT: 使用灰色(114,114,114)填充，与Python保持一致
    // 黑色(0,0,0)会导致小图像识别失败
    const cv::Scalar PAD_COLOR(114, 114, 114);
    
    if (orig_ratio < target_ratio) {
        // 图像比目标窄 -> 右侧补边（与Python一致）
        int new_width = static_cast<int>(orig_h * target_ratio);
        int pad_w = new_width - orig_w;
        cv::copyMakeBorder(image, padded, 0, 0, 0, pad_w,
                          cv::BORDER_CONSTANT, PAD_COLOR);
    } else {
        // orig_ratio >= target_ratio: 图像比目标宽或相等
        // Python在这种情况下不做padding，直接使用原图
        // 与Python保持一致！
        padded = image.clone();
    }
    
    // Step 2: Resize到目标尺寸
    cv::Mat resized;
    cv::resize(padded, resized, cv::Size(target_width, target_height));
    
    // NPU模式：不做归一化，也不做transpose！
    // DXRT引擎内部会处理NHWC->NCHW的转换
    // 输入应该是HWC格式的uint8 [0-255]
    
    // 确保连续内存
    if (!resized.isContinuous()) {
        resized = resized.clone();
    }
    
    LOG_DEBUG("Preprocessed: input %dx%d -> padded %dx%d -> resized %dx%d HWC uint8",
              image.cols, image.rows, padded.cols, padded.rows, target_width, target_height);
    
    return resized;
}

std::pair<std::string, float> TextRecognizer::Postprocess(dxrt::TensorPtrs& outputs) {
    if (outputs.empty()) {
        LOG_ERROR("Empty output tensors");
        return {"", 0.0f};
    }
    
    // 使用CTC解码器
    return decoder_->decode(outputs[0]);
}

void TextRecognizer::PrintModelUsageStats() const {
    LOG_DEBUG("=== Recognition Model Usage Statistics ===");
    int total = 0;
    for (const auto& [ratio, count] : model_usage_) {
        total += count;
    }
    
    if (total == 0) {
        LOG_DEBUG("No models used yet");
        return;
    }
    
    LOG_DEBUG_EXEC(([&]{
        for (const auto& [ratio, count] : model_usage_) {
            LOG_DEBUG("  ratio_%d: %d times (%.1f%%)", ratio, count, (count * 100.0f) / total);
        }
        LOG_DEBUG("  Total: %d recognitions", total);
    }));
}

} // namespace DeepXOCR

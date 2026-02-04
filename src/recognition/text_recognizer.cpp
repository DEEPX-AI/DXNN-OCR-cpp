#include "recognition/text_recognizer.h"
#include "recognition/rec_postprocess.h"
#include "preprocessing/image_ops.h"
#include "common/geometry.h"
#include "common/logger.hpp"
#include <algorithm>
#include <cmath>

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
            LOG_INFO("  Loaded ratio_{} model: {}", ratio, model_path);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to load ratio_{} model: {}", ratio, e.what());
            return false;
        }
    }
    
    if (models_.empty()) {
        LOG_ERROR("No recognition models loaded");
        return false;
    }
    
    // 加载字符字典
    LOG_INFO("Loading character dictionary from: {}", config_.dictPath);
    decoder_ = std::make_unique<ocr::CTCDecoder>(config_.dictPath, true);
    
    if (decoder_->getDictSize() == 0) {
        LOG_ERROR("Failed to load character dictionary");
        return false;
    }
    
    LOG_INFO("TextRecognizer initialized successfully");
    LOG_INFO("  Models: {} ratios", models_.size());
    LOG_INFO("  Dictionary: {} characters", decoder_->getDictSize());
    
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
        LOG_ERROR("No suitable model for image size {}x{}", 
                  textImage.cols, textImage.rows);
        return {"", 0.0f};
    }
    
    // 获取ratio
    int ratio = CalculateRatio(textImage.cols, textImage.rows);
    
    LOG_DEBUG("Recognize: input size={}x{}, calculated ratio={}, using model=ratio_{}",
              textImage.cols, textImage.rows, 
              textImage.cols / (textImage.rows > 0 ? textImage.rows : 1), ratio);
    
    // 预处理
    auto t1 = std::chrono::high_resolution_clock::now();
    cv::Mat preprocessed = Preprocess(textImage, ratio);
    auto t2 = std::chrono::high_resolution_clock::now();
    if (preprocessed.empty()) {
        LOG_ERROR("Preprocessing failed");
        return {"", 0.0f};
    }
    
    LOG_DEBUG("Preprocessed: size={}x{}, type={}, depth={}, channels={}, elemSize={}",
              preprocessed.cols, preprocessed.rows, preprocessed.type(), 
              preprocessed.depth(), preprocessed.channels(), preprocessed.elemSize());
    
    // Debug: check if data is valid
    if (preprocessed.data == nullptr) {
        LOG_ERROR("Preprocessed data is nullptr!");
        return {"", 0.0f};
    }
    
    // 推理
    auto outputs = engine->Run(preprocessed.data);
    auto t3 = std::chrono::high_resolution_clock::now();
    if (outputs.empty()) {
        LOG_ERROR("Inference failed: no output tensors");
        return {"", 0.0f};
    }
    
    // 后处理 (CTC解码)
    auto result = Postprocess(outputs);
    auto t4 = std::chrono::high_resolution_clock::now();
    
    // 累加计时
    last_preprocess_time_ += std::chrono::duration<double, std::milli>(t2 - t1).count();
    last_inference_time_ += std::chrono::duration<double, std::milli>(t3 - t2).count();
    last_postprocess_time_ += std::chrono::duration<double, std::milli>(t4 - t3).count();
    
    // 置信度过滤
    if (result.second < config_.confThreshold) {
        LOG_DEBUG("Low confidence FILTERED: text='{}', conf={:.4f} < threshold={:.4f}", 
                  result.first, result.second, config_.confThreshold);
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
    LOG_WARN("No exact model for ratio {}, using closest match", ratio);
    
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
        LOG_DEBUG("Using ratio_{} model instead", closest_ratio);
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
    // Recognition预处理 (完全按照Python来):
    // DXNN模型内部会自动处理归一化，所以只需要:
    // 1. resize (mode=ppocr, pad + resize)
    // 输出: HWC uint8 (DXNN内部会处理归一化)
    
    int target_height = config_.inputHeight;  // 48
    int target_width;
    
    // Python的映射：ratio_3 -> 120, ratio_5 -> 240, ratio_10 -> 480, ...
    if (ratio == 3) {
        target_width = 120;  // Special case: 120 instead of 144
    } else {
        target_width = target_height * ratio;
    }
    
    LOG_DEBUG("Preprocessing: {}x{} -> {}x{} (ratio_{})",
              image.cols, image.rows, target_width, target_height, ratio);
    
    // PPOCR style resize (pad + resize)
    int orig_h = image.rows;
    int orig_w = image.cols;
    
    float target_ratio = static_cast<float>(target_width) / target_height;
    float orig_ratio = static_cast<float>(orig_w) / orig_h;
    
    cv::Mat padded;
    
    // 使用灰色(114,114,114)填充，与Python保持一致
    const cv::Scalar PAD_COLOR(114, 114, 114);
    
    if (orig_ratio < target_ratio) {
        // 图像比目标窄 -> 右侧补边
        int new_width = static_cast<int>(orig_h * target_ratio);
        int pad_w = new_width - orig_w;
        cv::copyMakeBorder(image, padded, 0, 0, 0, pad_w,
                          cv::BORDER_CONSTANT, PAD_COLOR);
    } else {
        padded = image.clone();
    }
    
    // Resize到目标尺寸
    cv::Mat resized;
    cv::resize(padded, resized, cv::Size(target_width, target_height));
    
    // 确保连续内存和正确类型 (uint8 HWC)
    if (!resized.isContinuous()) {
        resized = resized.clone();
    }
    
    LOG_DEBUG("Preprocessed: input {}x{} -> padded {}x{} -> resized {}x{} HWC uint8",
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
        for ([[maybe_unused]] const auto& pair : model_usage_) {
            LOG_DEBUG("  ratio_{}: {} times ({:.1f}%)", pair.first, pair.second, (pair.second * 100.0f) / total);
        }
        LOG_DEBUG("  Total: {} recognitions", total);
    }));
}

// ==================== Async Recognition Implementation ====================

// Context for async recognition
struct RecognitionContext {
    cv::Mat preprocessed;  // Keep preprocessed image alive during async inference
    void* userArg;
};

void TextRecognizer::RegisterCallback(std::function<void(const std::string&, float, void*)> callback) {
    userCallback_ = callback;
    
    // Register internal callback with all models
    auto internalCb = [this](dxrt::TensorPtrs& outputs, void* userArg) {
        return this->internalCallback(outputs, userArg);
    };
    
    for (auto& [ratio, model] : models_) {
        model->RegisterCallback(internalCb);
        LOG_DEBUG("Registered async callback for ratio_{} model", ratio);
    }
}

int TextRecognizer::RecognizeAsync(const cv::Mat& textImage, void* userArg) {
    if (textImage.empty()) {
        LOG_ERROR("Input image is empty");
        if (userCallback_) {
            userCallback_("", 0.0f, userArg);
        }
        return -1;
    }
    
    // Select appropriate model
    auto* engine = SelectModel(textImage);
    if (!engine) {
        LOG_ERROR("No suitable model for image size {}x{}", 
                  textImage.cols, textImage.rows);
        if (userCallback_) {
            userCallback_("", 0.0f, userArg);
        }
        return -1;
    }
    
    // Get ratio and preprocess
    int ratio = CalculateRatio(textImage.cols, textImage.rows);
    cv::Mat preprocessed = Preprocess(textImage, ratio);
    
    if (preprocessed.empty()) {
        LOG_ERROR("Preprocessing failed");
        if (userCallback_) {
            userCallback_("", 0.0f, userArg);
        }
        return -1;
    }
    
    // Ensure continuous memory
    if (!preprocessed.isContinuous()) {
        preprocessed = preprocessed.clone();
    }
    
    // Create context - store preprocessed image to keep it alive during async inference
    RecognitionContext* ctx = new RecognitionContext{preprocessed.clone(), userArg};
    
    // Submit async inference (use preprocessed.data directly, same as sync version)
    engine->RunAsync(ctx->preprocessed.data, ctx);
    
    return 0;
}

int TextRecognizer::internalCallback(dxrt::TensorPtrs& outputs, void* userArg) {
    RecognitionContext* ctx = static_cast<RecognitionContext*>(userArg);
    if (!ctx) {
        // This is expected when synchronous Run() triggers the async callback
        // Sync calls handle results directly from Run() return value
        LOG_DEBUG("Recognition callback: null context (sync call, ignoring)");
        return 0;  // Return success, not error
    }
    
    // Ensure context is deleted
    std::unique_ptr<RecognitionContext> ctxGuard(ctx);
    
    if (outputs.empty()) {
        LOG_ERROR("Recognition inference failed: no output tensors");
        if (userCallback_) {
            userCallback_("", 0.0f, ctx->userArg);
        }
        return -1;
    }
    
    // Debug: log output tensor info
    auto& tensor = outputs[0];
    auto shape = tensor->shape();
    LOG_DEBUG("Recognition output: shape=[{}], size={}", 
              shape.size() > 0 ? std::to_string(shape[0]) + (shape.size() > 1 ? "," + std::to_string(shape[1]) : "") + (shape.size() > 2 ? "," + std::to_string(shape[2]) : "") : "empty",
              tensor->size());
    
    // Postprocess (CTC decode)
    auto [text, confidence] = Postprocess(outputs);
    
    LOG_DEBUG("Recognition result: text='{}', conf={:.4f}", text.empty() ? "<empty>" : text.substr(0, 30), confidence);
    
    // Apply confidence threshold
    if (confidence < config_.confThreshold) {
        LOG_DEBUG("Low confidence filtered: text='{}', conf={:.4f}", text, confidence);
        text = "";
    }
    
    // Invoke user callback
    if (userCallback_) {
        userCallback_(text, confidence, ctx->userArg);
    }
    
    return 0;
}

} // namespace DeepXOCR

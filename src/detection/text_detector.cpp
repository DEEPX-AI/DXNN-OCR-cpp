#include "detection/text_detector.h"
#include "common/logger.hpp"
#include "detection/db_postprocess.h"
#include "preprocessing/image_ops.h"
#include "common/visualizer.h"
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>

namespace ocr {

// Helper function to create directory
static void createDirectory(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

void DetectorConfig::Show() const {
    LOG_INFO("DetectorConfig:");
    LOG_INFO("  thresh={:.2f}, boxThresh={:.2f}, unclipRatio={:.2f}",
             thresh, boxThresh, unclipRatio);
    LOG_INFO("  model640={}", model640Path);
    LOG_INFO("  model960={}", model960Path);
}

TextDetector::TextDetector(const DetectorConfig& config)
    : config_(config) {
}

TextDetector::~TextDetector() {
}

bool TextDetector::init() {
    if (initialized_) {
        LOG_WARN("TextDetector already initialized");
        return true;
    }

    // Create postprocessor
    postprocessor_ = std::make_unique<DBPostProcessor>(
        config_.thresh,
        config_.boxThresh,
        config_.maxCandidates,
        config_.unclipRatio
    );

    // Load models
    try {
        LOG_INFO("Loading detection models...");
        
        auto cb = [this](dxrt::TensorPtrs& outputs, void* userArg) {
            return this->internalCallback(outputs, userArg);
        };

        // Load 640 model
        if (!config_.model640Path.empty()) {
            model640_ = std::make_unique<dxrt::InferenceEngine>(config_.model640Path);
            model640_->RegisterCallback(cb);
            LOG_INFO("Loaded det_640 model: {}", config_.model640Path);
        }
        
        // Load 960 model
        if (!config_.model960Path.empty()) {
            model960_ = std::make_unique<dxrt::InferenceEngine>(config_.model960Path);
            model960_->RegisterCallback(cb);
            LOG_INFO("Loaded det_960 model: {}", config_.model960Path);
        }

        if (!model640_ && !model960_) {
            LOG_ERROR("No detection model loaded");
            return false;
        }

        initialized_ = true;
        LOG_INFO("TextDetector initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize TextDetector: {}", e.what());
        return false;
    }
}

std::vector<DeepXOCR::TextBox> TextDetector::detect(const cv::Mat& image) {
    if (!initialized_) {
        LOG_ERROR("TextDetector not initialized");
        return {};
    }

    if (image.empty()) {
        LOG_ERROR("Input image is empty");
        return {};
    }

    int orig_h = image.rows;
    int orig_w = image.cols;

    // Select model based on image size
    auto* engine = selectModel(orig_h, orig_w);
    if (!engine) {
        LOG_ERROR("No suitable model for image size {}x{}", orig_h, orig_w);
        return {};
    }

    // Determine target size
    int target_size = getTargetSize(orig_h, orig_w);

    // === Stage 1: Preprocessing ===
    auto t1 = std::chrono::high_resolution_clock::now();
    int resized_h, resized_w;
    cv::Mat preprocessed = preprocess(image, target_size, resized_h, resized_w);
    auto t2 = std::chrono::high_resolution_clock::now();
    double preprocess_time = std::chrono::duration<double, std::milli>(t2 - t1).count();

    // === Stage 2: Model Inference ===
    cv::Mat pred = runInference(engine, preprocessed);
    auto t3 = std::chrono::high_resolution_clock::now();
    double inference_time = std::chrono::duration<double, std::milli>(t3 - t2).count();
    
    if (pred.empty()) {
        LOG_ERROR("Inference returned empty result");
        return {};
    }

    // === Stage 3: Postprocessing ===
    auto boxes = postprocessor_->process(pred, orig_h, orig_w, resized_h, resized_w);
    auto t4 = std::chrono::high_resolution_clock::now();
    double postprocess_time = std::chrono::duration<double, std::milli>(t4 - t3).count();
    
    // Save timing details
    last_preprocess_time_ = preprocess_time;
    last_inference_time_ = inference_time;
    last_postprocess_time_ = postprocess_time;
    
    LOG_INFO("Detection: {} boxes | Preprocess: {:.2f}ms | Inference: {:.2f}ms | Postprocess: {:.2f}ms", 
             boxes.size(), preprocess_time, inference_time, postprocess_time);
    
    // Save final detection result if enabled
    if (config_.saveIntermediates && !boxes.empty()) {
        createDirectory(config_.outputDir);
        cv::Mat vis_image = image.clone();
        Visualizer::drawTextBoxes(vis_image, boxes);
        std::string path = config_.outputDir + "/detection_result.jpg";
        cv::imwrite(path, vis_image);
        LOG_INFO("Saved detection result: {}", path);
    }
    
    return boxes;
}

int TextDetector::getTargetSize(int height, int width) {
    auto* engine = selectModel(height, width);
    if (!engine) {
        // Fallback logic if no model loaded (shouldn't happen if init succeeded)
        return (std::max(height, width) < config_.sizeThreshold) ? 640 : 960;
    }
    return (engine == model640_.get()) ? 640 : 960;
}

dxrt::InferenceEngine* TextDetector::selectModel(int height, int width) {
    int max_side = std::max(height, width);

    if (max_side < config_.sizeThreshold && model640_) {
        LOG_DEBUG("Using 640 model for image size {}x{}", height, width);
        return model640_.get();
    } else if (model960_) {
        LOG_DEBUG("Using 960 model for image size {}x{}", height, width);
        return model960_.get();
    } else if (model640_) {
        return model640_.get();
    }

    return nullptr;
}

cv::Mat TextDetector::preprocess(const cv::Mat& image, int target_size,
                                 int& resized_h, int& resized_w) {
    // PPOCR preprocessing: Pad first to square ratio, then resize
    // This is critical for correct coordinate mapping!
    
    int orig_h = image.rows;
    int orig_w = image.cols;
    
    // Step 1: Calculate target ratio (square for detection)
    float target_ratio = 1.0f;  // target_size x target_size is square
    float orig_ratio = static_cast<float>(orig_w) / orig_h;
    
    cv::Mat padded;
    int pad_h = 0, pad_w = 0;
    
    // IMPORTANT: 使用灰色(114,114,114)填充，与Python保持一致
    // 黑色(0,0,0)会导致边缘识别失败
    const cv::Scalar PAD_COLOR(114, 114, 114);
    
    if (orig_ratio < target_ratio) {
        // Image is taller than square ratio -> pad width
        int new_width = static_cast<int>(orig_h * target_ratio);
        pad_w = new_width - orig_w;
        // Pad on right (matching Python's left=0, right=pad_width)
        cv::copyMakeBorder(image, padded, 0, 0, 0, pad_w,
                          cv::BORDER_CONSTANT, PAD_COLOR);
    } else if (orig_ratio > target_ratio) {
        // Image is wider than square ratio -> pad height
        int new_height = static_cast<int>(orig_w / target_ratio);
        pad_h = new_height - orig_h;
        // Pad on bottom (matching Python's top=0, bottom=pad_height)
        cv::copyMakeBorder(image, padded, 0, pad_h, 0, 0,
                          cv::BORDER_CONSTANT, PAD_COLOR);
    } else {
        // Already square
        padded = image.clone();
    }
    
    // Step 2: Resize the padded image to target_size x target_size
    cv::Mat final_image;
    cv::resize(padded, final_image, cv::Size(target_size, target_size));
    
    // Store padded dimensions (before resize) for coordinate mapping
    resized_h = padded.rows;
    resized_w = padded.cols;
    
    LOG_DEBUG("PPOCR Preprocess: original {}x{} -> padded {}x{} (pad_h={}, pad_w={}) -> resized {}x{}",
              orig_w, orig_h, padded.cols, padded.rows, pad_h, pad_w, target_size, target_size);
    
    return final_image;
}

cv::Mat TextDetector::preprocessAsync(const cv::Mat& image, int target_size, int& resized_h, int& resized_w) {
    return preprocess(image, target_size, resized_h, resized_w);
}

void TextDetector::setCallback(DetectionCallback callback) {
    userCallback_ = callback;
}

int TextDetector::runAsync(const cv::Mat& input, int orig_h, int orig_w, int resized_h, int resized_w, int64_t taskId, const cv::Mat& originalImage, double preprocess_time) {
    auto* engine = selectModel(orig_h, orig_w);
    if (!engine) return -1;

    if (!input.isContinuous()) {
        LOG_ERROR("Async inference requires continuous input memory");
        return -1;
    }

    // Create context - CLONE input and originalImage to ensure they stay valid during async inference
    DetectionContext* ctx = new DetectionContext{
        orig_h, orig_w,
        resized_h, resized_w,  // Use the correct padded dimensions for coordinate mapping
        taskId,
        originalImage.clone(),  // Deep copy to keep image alive
        input.clone(),          // Deep copy to keep input data alive
        preprocess_time
    };

    // Use ctx->inputImage.data to ensure we're using the cloned data
    engine->RunAsync(ctx->inputImage.data, ctx);
    return 0;
}

int TextDetector::internalCallback(dxrt::TensorPtrs& outputs, void* userArg) {
    DetectionContext* ctx = static_cast<DetectionContext*>(userArg);
    if (!ctx) return -1;

    // Ensure context is deleted
    std::unique_ptr<DetectionContext> ctxGuard(ctx);

    if (outputs.empty()) {
        LOG_ERROR("Inference failed: no output tensors");
        return -1;
    }
    
    auto& output_tensor = outputs[0];
    if (!output_tensor) {
        LOG_ERROR("Output tensor is null");
        return -1;
    }
    
    auto shape = output_tensor->shape();
    if (shape.size() != 4) {
        LOG_ERROR("Unexpected output shape size: {}", shape.size());
        return -1;
    }

    int out_h = shape[2];
    int out_w = shape[3];
    cv::Mat pred(out_h, out_w, CV_32FC1);
    std::memcpy(pred.data, output_tensor->data(), out_h * out_w * sizeof(float));
    
    // Postprocess
    auto t_start = std::chrono::high_resolution_clock::now();
    auto boxes = postprocessor_->process(pred, ctx->orig_h, ctx->orig_w, ctx->resized_h, ctx->resized_w);
    auto t_end = std::chrono::high_resolution_clock::now();
    double postprocess_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Calculate inference time (approximate)
    double inference_time = 0.0; 

    if (userCallback_) {
        userCallback_(boxes, ctx->taskId, ctx->originalImage, ctx->preprocess_time, inference_time, postprocess_time);
    }

    return 0;
}

cv::Mat TextDetector::runInference(dxrt::InferenceEngine* engine, const cv::Mat& input) {    
    // Ensure contiguous memory: avoid cloning when not necessary
    const uint8_t* input_ptr = nullptr;
    cv::Mat contiguous;
    if (input.isContinuous()) {
        input_ptr = input.ptr<uint8_t>();
    } else {
        // only clone when input is not contiguous
        contiguous = input.clone();
        input_ptr = contiguous.ptr<uint8_t>();
    }

    // Run inference with uint8 HWC data and measure time
    auto t_start = std::chrono::high_resolution_clock::now();
    auto outputs = engine->Run(reinterpret_cast<void*>(const_cast<uint8_t*>(input_ptr)));
    auto t_end = std::chrono::high_resolution_clock::now();
    double run_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    LOG_INFO("Engine Run time: {:.2f}ms", run_time);

    if (outputs.empty()) {
        LOG_ERROR("Inference failed: no output tensors");
        return cv::Mat();
    }

    // Get output tensor
    auto output_tensor = outputs[0];
    auto shape = output_tensor->shape();
    
    // Shape should be [1, 1, H, W] for detection
    if (shape.size() != 4) {
        LOG_ERROR("Unexpected output shape size: {}", shape.size());
        return cv::Mat();
    }

    int out_h = shape[2];
    int out_w = shape[3];

    // Convert output to cv::Mat
    cv::Mat pred(out_h, out_w, CV_32FC1);
    const float* output_data = reinterpret_cast<const float*>(output_tensor->data());

    // Fast copy: memcpy entire buffer (pred is continuous by construction)
    size_t total = static_cast<size_t>(out_h) * static_cast<size_t>(out_w);
    std::memcpy(pred.data, reinterpret_cast<const void*>(output_data), total * sizeof(float));
    LOG_DEBUG("Copied {} floats into cv::Mat via memcpy", total);

    return pred;
}

} // namespace ocr

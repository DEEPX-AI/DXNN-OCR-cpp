#include "detection/text_detector.h"
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
    LOG_INFO("  thresh=%.2f, boxThresh=%.2f, unclipRatio=%.2f",
             thresh, boxThresh, unclipRatio);
    LOG_INFO("  model640=%s", model640Path.c_str());
    LOG_INFO("  model960=%s", model960Path.c_str());
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
        
        // Load 640 model
        if (!config_.model640Path.empty()) {
            model640_ = std::make_unique<dxrt::InferenceEngine>(config_.model640Path);
            LOG_INFO("Loaded det_640 model: %s", config_.model640Path.c_str());
        }
        
        // Load 960 model
        if (!config_.model960Path.empty()) {
            model960_ = std::make_unique<dxrt::InferenceEngine>(config_.model960Path);
            LOG_INFO("Loaded det_960 model: %s", config_.model960Path.c_str());
        }

        if (!model640_ && !model960_) {
            LOG_ERROR("No detection model loaded");
            return false;
        }

        initialized_ = true;
        LOG_INFO("TextDetector initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize TextDetector: %s", e.what());
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
        LOG_ERROR("No suitable model for image size %dx%d", orig_h, orig_w);
        return {};
    }

    // Determine target size based on selected model
    int target_size = (engine == model640_.get()) ? 640 : 960;

    // === Stage 1: Preprocessing ===
    int resized_h, resized_w;
    cv::Mat preprocessed = preprocess(image, target_size, resized_h, resized_w);

    // === Stage 2: Model Inference ===
    cv::Mat pred = runInference(engine, preprocessed);
    
    if (pred.empty()) {
        LOG_ERROR("Inference returned empty result");
        return {};
    }

    // === Stage 3: Postprocessing ===
    auto boxes = postprocessor_->process(pred, orig_h, orig_w, resized_h, resized_w);
    LOG_INFO("Detected %zu text boxes", boxes.size());
    
    // Save final detection result if enabled
    if (config_.saveIntermediates && !boxes.empty()) {
        createDirectory(config_.outputDir);
        cv::Mat vis_image = image.clone();
        Visualizer::drawTextBoxes(vis_image, boxes);
        std::string path = config_.outputDir + "/detection_result.jpg";
        cv::imwrite(path, vis_image);
        LOG_INFO("Saved detection result: %s", path.c_str());
    }
    
    return boxes;
}

dxrt::InferenceEngine* TextDetector::selectModel(int height, int width) {
    int max_side = std::max(height, width);

    if (max_side < config_.sizeThreshold && model640_) {
        LOG_DEBUG("Using 640 model for image size %dx%d", height, width);
        return model640_.get();
    } else if (model960_) {
        LOG_DEBUG("Using 960 model for image size %dx%d", height, width);
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
    
    LOG_DEBUG("PPOCR Preprocess: original %dx%d -> padded %dx%d (pad_h=%d, pad_w=%d) -> resized %dx%d",
              orig_w, orig_h, padded.cols, padded.rows, pad_h, pad_w, target_size, target_size);
    
    return final_image;
}

cv::Mat TextDetector::runInference(dxrt::InferenceEngine* engine, const cv::Mat& input) {
    // DXRT expects contiguous uint8 data in HWC format
    int h = input.rows;
    int w = input.cols;
    int c = input.channels();
    
    // Check input size
    size_t expected_size = engine->GetInputSize();
    size_t actual_size = h * w * c;
    
    LOG_DEBUG("Input: %dx%dx%d (HWC, uint8), actual size: %zu bytes, expected: %zu bytes", 
              h, w, c, actual_size, expected_size);
    
    if (actual_size != expected_size) {
        LOG_ERROR("Input size mismatch! Expected %zu but got %zu", expected_size, actual_size);
        return cv::Mat();
    }
    
    // Ensure contiguous memory
    cv::Mat contiguous = input.clone();
    
    // Run inference with uint8 HWC data
    auto outputs = engine->Run(contiguous.data);

    if (outputs.empty()) {
        LOG_ERROR("Inference failed: no output tensors");
        return cv::Mat();
    }

    // Get output tensor
    auto output_tensor = outputs[0];
    auto shape = output_tensor->shape();
    
    LOG_DEBUG("Output shape: [%zu, %zu, %zu, %zu]", 
              shape.size() > 0 ? static_cast<size_t>(shape[0]) : 0,
              shape.size() > 1 ? static_cast<size_t>(shape[1]) : 0,
              shape.size() > 2 ? static_cast<size_t>(shape[2]) : 0,
              shape.size() > 3 ? static_cast<size_t>(shape[3]) : 0);
    
    // Shape should be [1, 1, H, W] for detection
    if (shape.size() != 4) {
        LOG_ERROR("Unexpected output shape size: %zu", shape.size());
        return cv::Mat();
    }

    int out_h = shape[2];
    int out_w = shape[3];

    // Convert output to cv::Mat
    cv::Mat pred(out_h, out_w, CV_32FC1);
    const float* output_data = reinterpret_cast<const float*>(output_tensor->data());
    
    for (int i = 0; i < out_h; i++) {
        for (int j = 0; j < out_w; j++) {
            float val = output_data[i * out_w + j];
            // DBNet output is already probability (no sigmoid needed)
            pred.at<float>(i, j) = val;
        }
    }
    
    // Check output data statistics (debug only)
    LOG_DEBUG_EXEC(([&]{
        float min_val = FLT_MAX;
        float max_val = -FLT_MAX;
        float sum = 0.0f;
        int total_pixels = out_h * out_w;
        
        for (int i = 0; i < out_h; i++) {
            for (int j = 0; j < out_w; j++) {
                float val = pred.at<float>(i, j);
                min_val = std::min(min_val, val);
                max_val = std::max(max_val, val);
                sum += val;
            }
        }
        
        LOG_DEBUG("Output statistics: min=%.4f max=%.4f mean=%.4f", min_val, max_val, sum / total_pixels);
    }));

    return pred;
}

} // namespace ocr

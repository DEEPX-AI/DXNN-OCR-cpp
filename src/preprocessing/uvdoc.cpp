/**
 * @file uvdoc.cpp
 * @brief UVDoc Document Unwarping Implementation
 */

#include "preprocessing/uvdoc.h"
#include "common/logger.hpp"
#include <chrono>

namespace ocr {

void UVDocConfig::Show() const {
    LOG_INFO("UVDocConfig:");
    LOG_INFO("  modelPath={}", modelPath);
    LOG_INFO("  inputSize={}x{}", inputWidth, inputHeight);
    LOG_INFO("  alignCorners={}", alignCorners ? "true" : "false");
}

UVDocProcessor::UVDocProcessor(const UVDocConfig& config)
    : config_(config) {
}

UVDocProcessor::~UVDocProcessor() {
    if (engine_) {
        delete engine_;
        engine_ = nullptr;
    }
    LOG_INFO("[~UVDocProcessor] UVDocProcessor destroyed");
}

bool UVDocProcessor::LoadModel() {
    if (modelLoaded_) {
        LOG_WARN("[LoadModel] Model already loaded");
        return true;
    }
    
    if (config_.modelPath.empty()) {
        LOG_ERROR("[LoadModel] Model path is empty");
        return false;
    }
    
    try {
        LOG_INFO("[LoadModel] Loading UVDoc model from: {}", config_.modelPath);
        engine_ = new dxrt::InferenceEngine(config_.modelPath);
        modelLoaded_ = true;
        LOG_INFO("[LoadModel] UVDoc model loaded successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[LoadModel] Failed to load model: {}", e.what());
        return false;
    }
}

cv::Mat UVDocProcessor::Preprocess(const cv::Mat& image) {
    // UVDoc preprocessing to match Python NPU mode (parse_npu_preprocessing_ops):
    // 
    // Python config: uvdoc_preprocess = [
    //     {"resize": {"size": [712, 488]}},  # size = [HEIGHT, WIDTH]  ← 注意！
    //     {"div": {"x": 255}},               # SKIPPED in NPU mode!
    //     {"transpose": {"axis": [2,0,1]}}   # HWC -> CHW, APPLIED
    // ]
    //
    // Python size=[712, 488] means:
    //   - size[0] = 712 = HEIGHT
    //   - size[1] = 488 = WIDTH
    //   - Resize to (H=712, W=488)
    //
    // Actual execution in NPU mode (is_ort=False):
    // 1. Resize: 原图 -> (height=712, width=488) HWC uint8
    // 2. Transpose: HWC -> CHW, output shape = (3, 712, 488) uint8
    //               where (C, H, W) = (3, 712, 488)
    // 3. NO normalization (/255) - keeps uint8 [0-255]
    //
    // Note: cv::resize uses cv::Size(width, height)
    
    LOG_DEBUG("[Preprocess] Input: {}x{} (WxH)", image.cols, image.rows);
    
    // Resize to (width=488, height=712)
    // Python: size=[712, 488] -> height=712, width=488
    // OpenCV: cv::Size(width, height) = (488, 712)
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(config_.inputWidth, config_.inputHeight));
    
    LOG_DEBUG("[Preprocess] Resized to: {}x{} (WxH), target was H={} W={}", 
              resized.cols, resized.rows, config_.inputHeight, config_.inputWidth);
    
    // Transpose HWC -> CHW to match Python's output
    // Python output: (3, 488, 712) where shape = (C, H, W)
    // This means: 3 channels, height=488, width=712
    std::vector<cv::Mat> channels(3);
    cv::split(resized, channels);
    
    // Create CHW data: shape [3, H, W] = [3, 488, 712]
    // Memory layout: all of channel 0, then all of channel 1, then all of channel 2
    cv::Mat chw_data(3, config_.inputHeight * config_.inputWidth, CV_8U);
    for (int c = 0; c < 3; ++c) {
        std::memcpy(chw_data.ptr<uint8_t>(c), channels[c].data, 
                   config_.inputHeight * config_.inputWidth);
    }
    
    // Reshape to [3, H, W] = [3, 488, 712]
    cv::Mat chw = chw_data.reshape(1, {3, config_.inputHeight, config_.inputWidth});
    
    // NO normalization - keep uint8 [0-255] to match Python NPU mode
    LOG_DEBUG("[Preprocess] Output: CHW shape=[3, {}, {}], dtype=uint8, NO normalization", 
              config_.inputHeight, config_.inputWidth);
    
    return chw;
}

float UVDocProcessor::Inference(const cv::Mat& preprocessed, cv::Mat& uvMap) {
    if (!engine_ || !modelLoaded_) {
        LOG_ERROR("[Inference] Model not loaded");
        return -1.0f;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Convert CHW uint8 [3, H, W] to NHWC uint8 [1, H, W, 3]
    // This matches Python's prepare_input: NCHW -> NHWC
    int C = preprocessed.size[0];  // 3
    int H = preprocessed.size[1];  // 712
    int W = preprocessed.size[2];  // 488
    
    // Create NHWC buffer
    std::vector<uint8_t> nhwc_data(1 * H * W * C);
    
    // Transpose CHW -> HWC, then add batch dimension
    for (int h = 0; h < H; ++h) {
        for (int w = 0; w < W; ++w) {
            for (int c = 0; c < C; ++c) {
                // CHW: data[c][h][w]
                // NHWC: data[0][h][w][c]
                int chw_idx = c * H * W + h * W + w;
                int nhwc_idx = h * W * C + w * C + c;
                nhwc_data[nhwc_idx] = preprocessed.data[chw_idx];
            }
        }
    }
    
    LOG_DEBUG("[Inference] Input prepared: NHWC [1, {}, {}, {}] uint8", H, W, C);
    
    // Run inference with NHWC uint8 data
    auto outputs = engine_->Run(nhwc_data.data());
    
    auto end = std::chrono::high_resolution_clock::now();
    float inferenceTime = std::chrono::duration<float, std::milli>(end - start).count();
    
    if (outputs.empty()) {
        LOG_ERROR("[Inference] No output from model");
        return -1.0f;
    }
    
    // Get output tensor
    auto output = outputs[0];
    auto output_shape = output->shape();
    const float* output_data = static_cast<const float*>(output->data());
    
    // Calculate output statistics
    int total_size = 1;
    for (auto dim : output_shape) {
        total_size *= dim;
    }
    
    LOG_DEBUG_EXEC(([&]{
        float sum = 0.0f;
        float minv = output_data[0];
        float maxv = output_data[0];
        for (int i = 0; i < total_size; ++i) {
            sum += output_data[i];
            if (output_data[i] < minv) minv = output_data[i];
            if (output_data[i] > maxv) maxv = output_data[i];
        }
        
        LOG_DEBUG("[Inference] Output: mean={:.4f} min={:.4f} max={:.4f}", sum / total_size, minv, maxv);
    }));
    
    // Output shape should be [1, 2, H, W] or [1, H, W, 2]
    // Check which format and extract dimensions
    int out_h, out_w;
    
    if (output_shape.size() == 4) {
        if (output_shape[1] == 2) {
            // NCHW format: [1, 2, H, W]
            out_h = output_shape[2];
            out_w = output_shape[3];
        } else {
            // NHWC format: [1, H, W, 2]
            out_h = output_shape[1];
            out_w = output_shape[2];
        }
    } else {
        LOG_ERROR("[Inference] Unexpected output shape size: {}", output_shape.size());
        return -1.0f;
    }
    
    LOG_DEBUG_EXEC(([&]{
        LOG_DEBUG("[Inference] Output shape: [{} {} {} {}] interpreted as: H={} W={} C={}", 
                  output_shape[0], output_shape[1], output_shape[2], output_shape[3],
                  out_h, out_w, (output_shape[1] == 2) ? output_shape[1] : output_shape[3]);
    }));
    
    // Create UV map as [2, H, W]
    uvMap = cv::Mat::zeros(2, out_h * out_w, CV_32F);
    
    // Convert from model output to CHW [2, H, W]
    int total_elements = out_h * out_w * 2;
    if (output_shape[1] == 2) {
        // Output is in NCHW format [1, 2, H, W]
        std::memcpy(uvMap.data, output_data, total_elements * sizeof(float));
    } else {
        // Output is in NHWC format [1, H, W, 2], need to convert to CHW
        for (int h = 0; h < out_h; ++h) {
            for (int w = 0; w < out_w; ++w) {
                int nhwc_idx = h * out_w * 2 + w * 2;
                int hw_idx = h * out_w + w;
                uvMap.at<float>(0, hw_idx) = output_data[nhwc_idx];     // U channel
                uvMap.at<float>(1, hw_idx) = output_data[nhwc_idx + 1]; // V channel
            }
        }
    }
    
    // Reshape to [2, H, W]
    uvMap = uvMap.reshape(1, {2, out_h, out_w});
    
    return inferenceTime;
}

cv::Mat UVDocProcessor::ResizeAlignCorners(const cv::Mat& image, const cv::Size& targetSize) {
    // PyTorch align_corners=True mode resize
    int src_h = image.rows;
    int src_w = image.cols;
    int target_h = targetSize.height;
    int target_w = targetSize.width;
    
    if (src_h == target_h && src_w == target_w) {
        return image.clone();
    }
    
    // Create coordinate grids
    cv::Mat x_coords(target_h, target_w, CV_32F);
    cv::Mat y_coords(target_h, target_w, CV_32F);
    
    float y_ratio = (target_h > 1) ? float(src_h - 1) / (target_h - 1) : 0.0f;
    float x_ratio = (target_w > 1) ? float(src_w - 1) / (target_w - 1) : 0.0f;
    
    for (int i = 0; i < target_h; ++i) {
        float y = i * y_ratio;
        for (int j = 0; j < target_w; ++j) {
            float x = j * x_ratio;
            x_coords.at<float>(i, j) = x;
            y_coords.at<float>(i, j) = y;
        }
    }
    
    // Use remap for bilinear interpolation
    cv::Mat result;
    cv::remap(image, result, x_coords, y_coords, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    
    return result;
}

cv::Mat UVDocProcessor::GridSample(const cv::Mat& image, const cv::Mat& uvMap, bool alignCorners) {
    // Grid sampling similar to PyTorch's F.grid_sample
    // uvMap: [2, H, W] containing normalized coordinates in [-1, 1]
    // image: [H, W, C] input image
    
    int out_h = uvMap.size[1];
    int out_w = uvMap.size[2];
    int channels = image.channels();
    
    cv::Mat result = cv::Mat::zeros(out_h, out_w, image.type());
    
    int img_h = image.rows;
    int img_w = image.cols;
    
    for (int i = 0; i < out_h; ++i) {
        for (int j = 0; j < out_w; ++j) {
            // Get normalized coordinates from UV map [-1, 1]
            float u = uvMap.at<float>(0, i * out_w + j);
            float v = uvMap.at<float>(1, i * out_w + j);
            
            // Convert from [-1, 1] to pixel coordinates
            float x, y;
            if (alignCorners) {
                x = ((u + 1.0f) / 2.0f) * (img_w - 1);
                y = ((v + 1.0f) / 2.0f) * (img_h - 1);
            } else {
                x = ((u + 1.0f) * img_w - 1.0f) / 2.0f;
                y = ((v + 1.0f) * img_h - 1.0f) / 2.0f;
            }
            
            // Check bounds
            if (x < 0 || x >= img_w - 1 || y < 0 || y >= img_h - 1) {
                continue; // Out of bounds, leave as zero (padding_mode='zeros')
            }
            
            // Bilinear interpolation
            int x0 = static_cast<int>(std::floor(x));
            int x1 = x0 + 1;
            int y0 = static_cast<int>(std::floor(y));
            int y1 = y0 + 1;
            
            float wx1 = x - x0;
            float wx0 = 1.0f - wx1;
            float wy1 = y - y0;
            float wy0 = 1.0f - wy1;
            
            for (int c = 0; c < channels; ++c) {
                float val = wy0 * wx0 * image.at<cv::Vec3b>(y0, x0)[c] +
                           wy0 * wx1 * image.at<cv::Vec3b>(y0, x1)[c] +
                           wy1 * wx0 * image.at<cv::Vec3b>(y1, x0)[c] +
                           wy1 * wx1 * image.at<cv::Vec3b>(y1, x1)[c];
                result.at<cv::Vec3b>(i, j)[c] = static_cast<uchar>(std::round(val));
            }
        }
    }
    
    return result;
}

cv::Mat UVDocProcessor::Postprocess(const cv::Mat& uvMap, const cv::Mat& originalImage) {
    // Resize UV map to match original image size
    int orig_h = originalImage.rows;
    int orig_w = originalImage.cols;
    
    // uvMap is [2, H, W], need to resize each channel
    cv::Mat u_channel(uvMap.size[1], uvMap.size[2], CV_32F, (void*)uvMap.data);
    cv::Mat v_channel(uvMap.size[1], uvMap.size[2], CV_32F, 
                     (void*)(uvMap.data + uvMap.size[1] * uvMap.size[2] * sizeof(float)));
    
    cv::Mat u_resized, v_resized;
    if (config_.alignCorners) {
        u_resized = ResizeAlignCorners(u_channel, cv::Size(orig_w, orig_h));
        v_resized = ResizeAlignCorners(v_channel, cv::Size(orig_w, orig_h));
    } else {
        cv::resize(u_channel, u_resized, cv::Size(orig_w, orig_h), 0, 0, cv::INTER_LINEAR);
        cv::resize(v_channel, v_resized, cv::Size(orig_w, orig_h), 0, 0, cv::INTER_LINEAR);
    }
    
    // Reconstruct UV map [2, H, W]
    cv::Mat uv_resized(2, orig_h * orig_w, CV_32F);
    std::memcpy(uv_resized.ptr<float>(0), u_resized.data, orig_h * orig_w * sizeof(float));
    std::memcpy(uv_resized.ptr<float>(1), v_resized.data, orig_h * orig_w * sizeof(float));
    uv_resized = uv_resized.reshape(1, {2, orig_h, orig_w});
    
    // Apply grid sampling to correct the image
    cv::Mat corrected = GridSample(originalImage, uv_resized, config_.alignCorners);
    
    return corrected;
}

UVDocResult UVDocProcessor::Process(const cv::Mat& image) {
    UVDocResult result;
    
    if (image.empty()) {
        LOG_ERROR("[Process] Input image is empty");
        return result;
    }
    
    if (!modelLoaded_) {
        LOG_ERROR("[Process] Model not loaded");
        return result;
    }
    
    // Preprocess
    cv::Mat preprocessed = Preprocess(image);
    
    // Inference to get UV displacement map
    cv::Mat uvMap;
    float inferenceTime = Inference(preprocessed, uvMap);
    
    if (inferenceTime < 0 || uvMap.empty()) {
        LOG_ERROR("[Process] Inference failed");
        return result;
    }
    
    // Postprocess: apply UV map to correct image
    result.correctedImage = Postprocess(uvMap, image);
    result.success = !result.correctedImage.empty();
    result.inferenceTime = inferenceTime;
    
    if (result.success) {
        LOG_DEBUG("[Process] UVDoc correction successful, inference time: {:.2f} ms", inferenceTime);
    } else {
        LOG_ERROR("[Process] Postprocessing failed");
    }
    
    return result;
}

} // namespace ocr

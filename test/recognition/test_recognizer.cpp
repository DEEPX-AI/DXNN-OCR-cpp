#include "recognition/text_recognizer.h"
#include "detection/text_detector.h"
#include "common/geometry.h"
#include "common/visualizer.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <locale>

namespace fs = std::filesystem;
using namespace DeepXOCR;
using namespace ocr;

int main(int /* argc */, char** /* argv */) {
    // 设置UTF-8编码
    std::locale::global(std::locale("en_US.UTF-8"));
    std::cout.imbue(std::locale());
    // 配置 - use absolute paths from PROJECT_ROOT_DIR
    std::string projectRoot = PROJECT_ROOT_DIR;
    std::string inputDir = projectRoot + "/test/test_images";
    std::string outputDir = projectRoot + "/test/recognition/results";
    
    // 创建输出目录
    fs::create_directories(outputDir);
    
    // ====================================
    // Step 1: 配置并初始化 Detection
    // ====================================
    LOG_INFO("=== Step 1: Initialize Detection ===");
    
    DetectorConfig det_config;
    det_config.model640Path = "engine/model_files/best/det_v5_640.dxnn";
    det_config.model960Path = "engine/model_files/best/det_v5_960.dxnn";
    det_config.thresh = 0.3f;
    det_config.boxThresh = 0.6f;
    det_config.maxCandidates = 1500;  // 与Python保持一致
    det_config.unclipRatio = 1.5f;
    
    TextDetector detector(det_config);
    if (!detector.init()) {
        LOG_ERROR("Failed to initialize detector");
        return -1;
    }
    
    // ====================================
    // Step 2: 配置并初始化 Recognition
    // ====================================
    LOG_INFO("\n=== Step 2: Initialize Recognition ===");
    
    RecognizerConfig rec_config;
    rec_config.dictPath = "engine/model_files/ppocrv5_dict.txt";
    rec_config.confThreshold = 0.3f;
    rec_config.inputHeight = 48;
    
    // 配置所有ratio模型
    rec_config.modelPaths = {
        {3,  "engine/model_files/best/rec_v5_ratio_3.dxnn"},
        {5,  "engine/model_files/best/rec_v5_ratio_5.dxnn"},
        {10, "engine/model_files/best/rec_v5_ratio_10.dxnn"},
        {15, "engine/model_files/best/rec_v5_ratio_15.dxnn"},
        {25, "engine/model_files/best/rec_v5_ratio_25.dxnn"},
        {35, "engine/model_files/best/rec_v5_ratio_35.dxnn"}
    };
    
    rec_config.Show();
    
    TextRecognizer recognizer(rec_config);
    if (!recognizer.Initialize()) {
        LOG_ERROR("Failed to initialize recognizer");
        return -1;
    }
    
    // ====================================
    // Step 3: 处理测试图片
    // ====================================
    LOG_INFO("\n=== Step 3: Processing Test Images ===");
    
    // 获取所有图片文件
    std::vector<fs::path> imageFiles;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".jpg" || ext == ".png" || ext == ".jpeg") {
                imageFiles.push_back(entry.path());
            }
        }
    }
    
    std::sort(imageFiles.begin(), imageFiles.end(), 
              [](const fs::path& a, const fs::path& b) {
                  return a.filename().string() < b.filename().string();
              });
    
    if (imageFiles.empty()) {
        LOG_ERROR("No images found in %s", inputDir.c_str());
        return -1;
    }
    
    LOG_INFO("Found %zu images", imageFiles.size());
    
    // 统计信息
    int totalImages = 0;
    int totalBoxes = 0;
    int totalRecognized = 0;
    double totalDetTime = 0;
    double totalRecTime = 0;
    double totalRecPostprocessTime = 0;  // 后处理总时间
    
    // 处理所有图片
    for (size_t i = 0; i < imageFiles.size(); i++) {
        const auto& imagePath = imageFiles[i];
        std::string filename = imagePath.filename().string();
        
        LOG_INFO("\n[%zu/%zu] Processing: %s", i+1, imageFiles.size(), filename.c_str());
        
        // 加载图像
        cv::Mat image = cv::imread(imagePath.string());
        if (image.empty()) {
            LOG_ERROR("  Failed to load image");
            continue;
        }
        
        totalImages++;
        
        // Step 3.1: 文本检测
        auto det_start = std::chrono::high_resolution_clock::now();
        auto boxes = detector.detect(image);
        auto det_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> det_time = det_end - det_start;
        totalDetTime += det_time.count();
        
        LOG_INFO("  Detected %zu boxes in %.2f ms", boxes.size(), det_time.count());
        totalBoxes += boxes.size();
        
        if (boxes.empty()) {
            continue;
        }
        
        // Step 3.2: 文本识别
        auto rec_start = std::chrono::high_resolution_clock::now();
        
        int imageRecognized = 0;
        for (size_t j = 0; j < boxes.size(); j++) {
            auto& box = boxes[j];
            
            // 裁剪文本区域
            std::vector<cv::Point2f> points = {
                box.points[0], box.points[1], box.points[2], box.points[3]
            };
            cv::Mat cropped = Geometry::getRotateCropImage(image, points);
            
            if (cropped.empty()) {
                continue;
            }
            
            // 识别文本
            auto [text, confidence] = recognizer.Recognize(cropped);
            
            // 更新结果
            box.text = text;
            box.confidence = confidence;
            
            if (!text.empty()) {
                imageRecognized++;
            }
        }
        
        auto rec_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> rec_time = rec_end - rec_start;
        totalRecTime += rec_time.count();
        totalRecognized += imageRecognized;
        totalRecPostprocessTime += rec_time.count();  // 后处理时间 = 识别总时间
        
        LOG_INFO("  Recognized %d/%zu boxes in %.2f ms", 
                 imageRecognized, boxes.size(), rec_time.count());
        
        // Step 3.3: 可视化结果（左右拼接：左边原图+检测框，右边文字）
        cv::Mat result = Visualizer::drawOCRResultsSideBySide(
            image, boxes, "engine/fonts/NotoSansCJK-Regular.ttc");
        std::string outputPath = outputDir + "/" + filename;
        cv::imwrite(outputPath, result);
        LOG_INFO("  Saved result to: %s", outputPath.c_str());
    }
    
    // ====================================
    // Step 4: 统计总结
    // ====================================
    LOG_INFO("\n=== Final Statistics ===");
    LOG_INFO("Total images: %d", totalImages);
    LOG_INFO("Total boxes detected: %d", totalBoxes);
    LOG_INFO("Total boxes recognized: %d", totalRecognized);
    LOG_INFO("Recognition rate: %.1f%%", 
             totalBoxes > 0 ? (100.0 * totalRecognized / totalBoxes) : 0.0);
    LOG_INFO("\n=== Performance Timing ===");
    LOG_INFO("Detection:");
    LOG_INFO("  Total: %.2f ms", totalDetTime);
    LOG_INFO("  Average: %.2f ms/image", 
             totalImages > 0 ? totalDetTime / totalImages : 0);
    LOG_INFO("\nRecognition Postprocess:");
    LOG_INFO("  Total: %.2f ms", totalRecPostprocessTime);
    LOG_INFO("  Average: %.2f ms/image", 
             totalImages > 0 ? totalRecPostprocessTime / totalImages : 0);
    LOG_INFO("  Average: %.2f ms/box",
             totalBoxes > 0 ? totalRecPostprocessTime / totalBoxes : 0);
    LOG_INFO("\nResults saved to: %s", outputDir.c_str());
    
    return 0;
}

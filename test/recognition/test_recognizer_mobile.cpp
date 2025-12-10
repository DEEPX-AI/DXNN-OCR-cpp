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
    std::string outputDir = projectRoot + "/test/recognition/results_mobile";
    
    // 创建输出目录
    fs::create_directories(outputDir);
    
    // ====================================
    // Step 1: 配置并初始化 Detection
    // ====================================
    LOG_INFO("=== Step 1: Initialize Detection ===");
    
    DetectorConfig det_config;
    det_config.model640Path = projectRoot + "/engine/model_files/mobile/det_mobile_640.dxnn";
    det_config.model960Path = projectRoot + "/engine/model_files/mobile/det_mobile_960.dxnn";
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
    // Step 2: 配置并初始化 Recognition (Mobile)
    // ====================================
    LOG_INFO("\n=== Step 2: Initialize Recognition (Mobile) ===");
    
    RecognizerConfig rec_config;
    rec_config.dictPath = projectRoot + "/engine/model_files/ppocrv5_dict.txt";
    rec_config.confThreshold = 0.3f;
    rec_config.inputHeight = 48;
    
    // 配置所有ratio模型 - Mobile版本
    rec_config.modelPaths = {
        {3,  projectRoot + "/engine/model_files/mobile/rec_mobile_ratio_3.dxnn"},
        {5,  projectRoot + "/engine/model_files/mobile/rec_mobile_ratio_5.dxnn"},
        {10, projectRoot + "/engine/model_files/mobile/rec_mobile_ratio_10.dxnn"},
        {15, projectRoot + "/engine/model_files/mobile/rec_mobile_ratio_15.dxnn"},
        {25, projectRoot + "/engine/model_files/mobile/rec_mobile_ratio_25.dxnn"},
        {35, projectRoot + "/engine/model_files/mobile/rec_mobile_ratio_35.dxnn"}
    };
    
    rec_config.Show();
    
    TextRecognizer recognizer(rec_config);
    if (!recognizer.Initialize()) {
        LOG_ERROR("Failed to initialize recognizer");
        return -1;
    }
    
    // ====================================
    // Step 3: 批量处理测试图像
    // ====================================
    LOG_INFO("\n=== Step 3: Processing Test Images ===");
    
    // 收集所有图像文件
    std::vector<std::string> imageFiles;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                imageFiles.push_back(entry.path().filename().string());
            }
        }
    }
    std::sort(imageFiles.begin(), imageFiles.end());
    
    LOG_INFO("Found {} images", imageFiles.size());
    
    // 统计信息
    double totalDetTime = 0.0;
    double totalRecTime = 0.0;
    int totalBoxes = 0;
    int totalRecognized = 0;
    double totalRecPreprocessTime = 0.0;
    double totalRecInferenceTime = 0.0;
    double totalRecPostprocessTime = 0.0;
    
    // 处理每个图像
    for (size_t i = 0; i < imageFiles.size(); i++) {
        const auto& filename = imageFiles[i];
        LOG_INFO("\n[{}/{}] Processing: {}", i+1, imageFiles.size(), filename);
        
        // Step 3.1: 文本检测
        std::string imagePath = inputDir + "/" + filename;
        cv::Mat image = cv::imread(imagePath);
        if (image.empty()) {
            LOG_ERROR("Failed to load image: {}", imagePath);
            continue;
        }
        
        auto det_start = std::chrono::high_resolution_clock::now();
        auto boxes = detector.detect(image);
        auto det_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> det_time = det_end - det_start;
        totalDetTime += det_time.count();
        
        LOG_INFO("  Detected {} boxes in {:.2f} ms", boxes.size(), det_time.count());
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
        
        LOG_INFO("  Recognized {}/{} boxes in {:.2f} ms", 
                 imageRecognized, boxes.size(), rec_time.count());
        
        // Step 3.3: 可视化结果（左右拼接：左边原图+检测框，右边文字）
        cv::Mat result = Visualizer::drawOCRResultsSideBySide(
            image, 
            boxes
        );
        
        // Step 3.4: 保存结果
        std::string outputPath = outputDir + "/" + filename;
        cv::imwrite(outputPath, result);
        LOG_INFO("  Saved result to: {}", outputPath);
    }
    
    // ====================================
    // Step 4: 打印统计信息
    // ====================================
    LOG_INFO("\n=== Final Statistics (Mobile) ===");
    LOG_INFO("Total Images: {}", imageFiles.size());
    LOG_INFO("Total Boxes Detected: {}", totalBoxes);
    LOG_INFO("Total Boxes Recognized: {}", totalRecognized);
    LOG_INFO("Recognition Rate: {:.2f}%", 
             totalBoxes > 0 ? (totalRecognized * 100.0 / totalBoxes) : 0.0);
    
    LOG_INFO("\nTiming:");
    LOG_INFO("  Detection Total: {:.2f} ms", totalDetTime);
    LOG_INFO("  Detection Average: {:.2f} ms/image", 
             totalDetTime / imageFiles.size());
    LOG_INFO("  Recognition Total: {:.2f} ms", totalRecTime);
    LOG_INFO("  Recognition Average: {:.2f} ms/image", 
             totalRecTime / imageFiles.size());
    LOG_INFO("  Recognition Average per Box: {:.2f} ms/box", 
             totalBoxes > 0 ? totalRecTime / totalBoxes : 0.0);
    
    LOG_INFO("\nRecognition Breakdown:");
    LOG_INFO("  Preprocess: {:.2f} ms", totalRecPreprocessTime);
    LOG_INFO("  Inference: {:.2f} ms", totalRecInferenceTime);
    LOG_INFO("  Postprocess: {:.2f} ms", totalRecPostprocessTime);
    
    // 打印模型使用统计
    recognizer.PrintModelUsageStats();
    
    LOG_INFO("\nResults saved to: {}", outputDir);
    
    return 0;
}

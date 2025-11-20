#include "pipeline/ocr_pipeline.h"
#include "common/logger.hpp"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>

namespace fs = std::filesystem;

/**
 * @brief 获取目录下所有图片文件
 */
std::vector<std::string> getImageFiles(const std::string& dirPath) {
    std::vector<std::string> imageFiles;
    
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        LOG_ERROR("Directory does not exist: %s", dirPath.c_str());
        return imageFiles;
    }
    
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
                imageFiles.push_back(entry.path().string());
            }
        }
    }
    
    std::sort(imageFiles.begin(), imageFiles.end());
    return imageFiles;
}

/**
 * @brief 打印OCR结果
 */
void printOCRResults(const std::string& imageName, 
                    const std::vector<ocr::PipelineOCRResult>& results,
                    const ocr::OCRPipelineStats& stats) {
    LOG_INFO("\n========== Image: %s ==========", imageName.c_str());
    stats.Show();
    
    if (!results.empty()) {
        LOG_INFO("\nOCR Results (sorted from top-left to bottom-right):");
        LOG_INFO("%-4s | %-50s | %s", "No.", "Text", "Conf");
        LOG_INFO("%s", std::string(70, '-').c_str());
        
        for (const auto& result : results) {
            LOG_INFO("%-4d | %-50s | %.3f", 
                    result.index + 1, 
                    result.text.c_str(), 
                    result.confidence);
        }
    }
    
    LOG_INFO(" ");
}

int main(int argc, char** argv) {
    // 解析命令行参数
    // 使用 PROJECT_ROOT_DIR 宏（在 CMakeLists.txt 中定义）
    std::string projectRoot = PROJECT_ROOT_DIR;
    std::string testImagesDir = projectRoot + "/test/test_images";
    std::string modelDir = projectRoot + "/engine/model_files";
    std::string outputDir = projectRoot + "/test/pipeline/results";
    
    if (argc >= 2) {
        testImagesDir = argv[1];
    }
    if (argc >= 3) {
        modelDir = argv[2];
    }
    if (argc >= 4) {
        outputDir = argv[3];
    }
    
    // 创建输出目录
    fs::create_directories(outputDir);
    
    LOG_INFO("========== OCR Pipeline Test ==========");
    LOG_INFO("Test Images Directory: %s", testImagesDir.c_str());
    LOG_INFO("Model Directory: %s", modelDir.c_str());
    LOG_INFO("Output Directory: %s", outputDir.c_str());
    LOG_INFO("=======================================\n");
    
    // 配置Pipeline
    ocr::OCRPipelineConfig config;
    
    // Detection配置
    config.detectorConfig.model640Path = modelDir + "/best/det_v5_640.dxnn";
    config.detectorConfig.model960Path = modelDir + "/best/det_v5_960.dxnn";
    config.detectorConfig.thresh = 0.3f;
    config.detectorConfig.boxThresh = 0.6f;
    config.detectorConfig.maxCandidates = 1500;
    config.detectorConfig.unclipRatio = 1.5f;
    
    // Recognition配置
    config.recognizerConfig.modelPaths = {
        {3, modelDir + "/best/rec_v5_ratio_3.dxnn"},
        {5, modelDir + "/best/rec_v5_ratio_5.dxnn"},
        {10, modelDir + "/best/rec_v5_ratio_10.dxnn"},
        {15, modelDir + "/best/rec_v5_ratio_15.dxnn"},
        {25, modelDir + "/best/rec_v5_ratio_25.dxnn"},
        {35, modelDir + "/best/rec_v5_ratio_35.dxnn"}
    };
    config.recognizerConfig.dictPath = modelDir + "/ppocrv5_dict.txt";
    config.recognizerConfig.confThreshold = 0.3f;
    config.recognizerConfig.inputHeight = 48;
    
    // Classification 配置
    config.classifierConfig.modelPath = modelDir + "/best/textline_ori.dxnn";
    config.classifierConfig.threshold = 0.9;
    config.classifierConfig.inputWidth = 160;
    config.classifierConfig.inputHeight = 80;
    config.useClassification = true;
    
    // Document Preprocessing 配置（统一管理）
    config.useDocPreprocessing = true;
    config.docPreprocessingConfig.useOrientation = true;
    config.docPreprocessingConfig.orientationConfig.modelPath = modelDir + "/best/doc_ori_fixed.dxnn";
    config.docPreprocessingConfig.orientationConfig.confidenceThreshold = 0.9f;  // 与Python保持一致
    
    config.docPreprocessingConfig.useUnwarping = true;
    config.docPreprocessingConfig.uvdocConfig.modelPath = modelDir + "/best/UVDoc_pruned_p3.dxnn";
    config.docPreprocessingConfig.uvdocConfig.inputWidth = 488;   // Python size=[712,488] -> width=488
    config.docPreprocessingConfig.uvdocConfig.inputHeight = 712;  // Python size=[712,488] -> height=712
    config.docPreprocessingConfig.uvdocConfig.alignCorners = true;
    
    // Pipeline配置
    config.enableVisualization = true;
    config.sortResults = true;
    
    // 显示配置
    config.Show();
    
    // 初始化Pipeline
    ocr::OCRPipeline pipeline(config);
    if (!pipeline.initialize()) {
        LOG_ERROR("Failed to initialize OCR Pipeline");
        return -1;
    }
    
    LOG_INFO("\n✅ OCR Pipeline initialized successfully!\n");
    
    // 获取测试图片
    std::vector<std::string> imageFiles = getImageFiles(testImagesDir);
    if (imageFiles.empty()) {
        LOG_ERROR("No image files found in: %s", testImagesDir.c_str());
        return -1;
    }
    
    LOG_INFO("Found %zu test images\n", imageFiles.size());
    
    // 处理每张图片
    int totalDetected = 0;
    int totalRecognized = 0;
    int totalRotated = 0;
    double totalDetTime = 0.0;
    double totalClsTime = 0.0;
    double totalRecTime = 0.0;
    double totalTime = 0.0;
    
    for (size_t i = 0; i < imageFiles.size(); ++i) {
        const std::string& imagePath = imageFiles[i];
        std::string imageName = fs::path(imagePath).filename().string();
        
        LOG_INFO("Processing [%zu/%zu]: %s", i + 1, imageFiles.size(), imageName.c_str());
        
        // 读取图片
        cv::Mat image = cv::imread(imagePath);
        if (image.empty()) {
            LOG_ERROR("Failed to read image: %s", imagePath.c_str());
            continue;
        }
        
        // 处理图片
        std::vector<ocr::PipelineOCRResult> results;
        cv::Mat visualImage;
        ocr::OCRPipelineStats stats;
        
        if (!pipeline.processWithVisualization(image, results, visualImage, &stats)) {
            LOG_ERROR("Failed to process image: %s", imageName.c_str());
            continue;
        }
        
        // 打印结果
        printOCRResults(imageName, results, stats);
        
        // 保存可视化结果
        std::string outputPath = outputDir + "/" + imageName;
        cv::imwrite(outputPath, visualImage);
        LOG_INFO("Saved visualization to: %s", outputPath.c_str());
        
        // 保存JSON结果
        std::string jsonPath = outputDir + "/" + 
            fs::path(imageName).stem().string() + ".json";
        ocr::OCRPipeline::saveResultsToJSON(results, jsonPath);
        
        // 累计统计
        totalDetected += stats.detectedBoxes;
        totalRecognized += stats.recognizedBoxes;
        totalRotated += stats.rotatedBoxes;
        totalDetTime += stats.detectionTime;
        totalClsTime += stats.classificationTime;
        totalRecTime += stats.recognitionTime;
        totalTime += stats.totalTime;
    }
    
    // 打印总体统计
    LOG_INFO("\n========== Overall Statistics ==========");
    LOG_INFO("Total Images: %zu", imageFiles.size());
    LOG_INFO("Total Detected Boxes: %d", totalDetected);
    LOG_INFO("Total Rotated Boxes: %d (%.1f%%)", totalRotated,
            totalDetected == 0 ? 0.0 : 
            (static_cast<double>(totalRotated) / totalDetected * 100.0));
    LOG_INFO("Total Recognized Boxes: %d", totalRecognized);
    LOG_INFO("Overall Recognition Rate: %.1f%%", 
            totalDetected == 0 ? 0.0 : 
            (static_cast<double>(totalRecognized) / totalDetected * 100.0));
    LOG_INFO("\nAverage Detection Time: %.2f ms/image", 
            totalDetTime / imageFiles.size());
    LOG_INFO("Average Classification Time: %.2f ms/image", 
            totalClsTime / imageFiles.size());
    LOG_INFO("Average Recognition Time: %.2f ms/image", 
            totalRecTime / imageFiles.size());
    LOG_INFO("Average Total Time: %.2f ms/image", 
            totalTime / imageFiles.size());
    LOG_INFO("\nTotal Processing Time: %.2f seconds", totalTime / 1000.0);
    LOG_INFO("========================================\n");
    
    LOG_INFO("✅ All tests completed! Results saved to: %s", outputDir.c_str());
    
    return 0;
}

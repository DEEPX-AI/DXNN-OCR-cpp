#include "detection/text_detector.h"
#include "common/visualizer.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace ocr;
using namespace DeepXOCR;

int main(int /* argc */, char** /* argv */) {
    // Configuration - use absolute paths from PROJECT_ROOT_DIR
    std::string projectRoot = PROJECT_ROOT_DIR;
    std::string inputDir = projectRoot + "/test/test_images";
    std::string outputDir = projectRoot + "/test/detection/results";
    
    // Create output directory
    fs::create_directories(outputDir);
    
    // Get all image files and sort them
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
    
    // Sort by filename
    std::sort(imageFiles.begin(), imageFiles.end(), 
              [](const fs::path& a, const fs::path& b) {
                  return a.filename().string() < b.filename().string();
              });
    
    if (imageFiles.empty()) {
        LOG_ERROR("No images found in %s", inputDir.c_str());
        return -1;
    }
    
    LOG_INFO("Found %zu images in %s", imageFiles.size(), inputDir.c_str());

    // Configure detector
    DetectorConfig config;
    config.model640Path = "engine/model_files/best/det_v5_640.dxnn";
    config.model960Path = "engine/model_files/best/det_v5_960.dxnn";
    config.thresh = 0.3f;
    config.boxThresh = 0.6f;
    config.maxCandidates = 1500;  // 与Python保持一致
    config.unclipRatio = 1.5f;
    config.unclipRatio = 1.5f;
    config.saveIntermediates = false;  // Don't save per-image results
    
    config.Show();

    // Create and initialize detector
    TextDetector detector(config);
    if (!detector.init()) {
        LOG_ERROR("Failed to initialize detector");
        return -1;
    }

    // Process all images
    LOG_INFO("Processing %zu images...", imageFiles.size());
    int successCount = 0;
    int failCount = 0;
    
    for (size_t i = 0; i < imageFiles.size(); ++i) {
        const auto& imagePath = imageFiles[i];
        std::string filename = imagePath.filename().string();
        
        LOG_INFO("[%zu/%zu] Processing: %s", i+1, imageFiles.size(), filename.c_str());
        
        // Load image
        cv::Mat image = cv::imread(imagePath.string());
        if (image.empty()) {
            LOG_ERROR("  Failed to load image");
            failCount++;
            continue;
        }
        
        // Detect text boxes (includes: preprocessing + inference + postprocessing)
        auto start = std::chrono::high_resolution_clock::now();
        auto boxes = detector.detect(image);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        
        LOG_INFO("  Detected %zu boxes in %.2f ms", boxes.size(), duration.count());
        
        // Draw boxes and save result (NOT included in timing)
        cv::Mat result = Visualizer::drawTextBoxes(image, boxes);
        
        std::string outputPath = outputDir + "/" + filename;
        cv::imwrite(outputPath, result);
        successCount++;
    }
    
    LOG_INFO("=====================================");
    LOG_INFO("Processing completed:");
    LOG_INFO("  Total: %zu images", imageFiles.size());
    LOG_INFO("  Success: %d images", successCount);
    LOG_INFO("  Failed: %d images", failCount);
    LOG_INFO("  Results saved to: %s", outputDir.c_str());
    LOG_INFO("=====================================");

    return 0;
}

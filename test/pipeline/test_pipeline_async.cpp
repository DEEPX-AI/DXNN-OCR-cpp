#include "pipeline/ocr_pipeline.h"
#include "common/logger.hpp"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

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

int main(int argc, char** argv) {
    std::string projectRoot = PROJECT_ROOT_DIR;
    std::string testImagesDir = projectRoot + "/test/test_images";
    std::string modelDir = projectRoot + "/engine/model_files";
    
    if (argc >= 2) testImagesDir = argv[1];
    if (argc >= 3) modelDir = argv[2];

    ocr::OCRPipelineConfig config;
    config.detectorConfig.model640Path = modelDir + "/best/det_v5_640.dxnn";
    config.detectorConfig.model960Path = modelDir + "/best/det_v5_960.dxnn";
    config.detectorConfig.thresh = 0.3f;
    config.detectorConfig.boxThresh = 0.6f;
    config.detectorConfig.maxCandidates = 1500;
    config.detectorConfig.unclipRatio = 1.5f;
    
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
    
    config.classifierConfig.modelPath = modelDir + "/best/textline_ori.dxnn";
    config.classifierConfig.threshold = 0.9;
    config.classifierConfig.inputWidth = 160;
    config.classifierConfig.inputHeight = 80;
    config.useClassification = true;
    
    config.useDocPreprocessing = true;
    config.docPreprocessingConfig.useOrientation = true;
    config.docPreprocessingConfig.orientationConfig.modelPath = modelDir + "/best/doc_ori_fixed.dxnn";
    config.docPreprocessingConfig.useUnwarping = true;
    config.docPreprocessingConfig.uvdocConfig.modelPath = modelDir + "/best/UVDoc_pruned_p3.dxnn";
    config.docPreprocessingConfig.uvdocConfig.inputWidth = 488;
    config.docPreprocessingConfig.uvdocConfig.inputHeight = 712;
    config.docPreprocessingConfig.uvdocConfig.alignCorners = true;
    
    config.enableVisualization = false; // Disable visualization for performance test
    config.sortResults = true;

    ocr::OCRPipeline pipeline(config);
    if (!pipeline.initialize()) {
        LOG_ERROR("Failed to initialize OCR Pipeline");
        return -1;
    }

    std::vector<std::string> imageFiles = getImageFiles(testImagesDir);
    if (imageFiles.empty()) {
        LOG_ERROR("No images found");
        return -1;
    }

    // Pre-load images to memory to measure pure pipeline performance
    std::vector<cv::Mat> images;
    images.reserve(imageFiles.size());
    for (const auto& path : imageFiles) {
        cv::Mat img = cv::imread(path);
        if (!img.empty()) images.push_back(img);
    }
    LOG_INFO("Loaded %zu images", images.size());

    // Start Async Pipeline
    pipeline.start();

    auto start_time = std::chrono::high_resolution_clock::now();
    std::atomic<int> completed_count{0};
    int total_images = images.size();

    // Consumer Thread
    std::thread consumer([&]() {
        std::vector<ocr::PipelineOCRResult> results;
        int64_t id;
        while (completed_count < total_images) {
            if (pipeline.getResult(results, id)) {
                completed_count++;
                if (completed_count % 10 == 0) {
                    LOG_INFO("Processed %d/%d", completed_count.load(), total_images);
                }
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer Loop
    for (int i = 0; i < total_images; ++i) {
        while (!pipeline.pushTask(images[i], i)) {
            // Queue full, wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    consumer.join();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    pipeline.stop();

    double total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    double fps = total_images / (total_time_ms / 1000.0);

    LOG_INFO("========== Async Performance ==========");
    LOG_INFO("Total Images: %d", total_images);
    LOG_INFO("Total Time: %.2f ms", total_time_ms);
    LOG_INFO("Average Time: %.2f ms/image", total_time_ms / total_images);
    LOG_INFO("FPS: %.2f", fps);
    LOG_INFO("=======================================");

    return 0;
}

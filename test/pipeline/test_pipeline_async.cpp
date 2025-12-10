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
        LOG_ERROR("Directory does not exist: {}", dirPath);
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
    config.detectorConfig.model640Path = modelDir + "/server/det_v5_640.dxnn";
    config.detectorConfig.model960Path = modelDir + "/server/det_v5_960.dxnn";
    config.detectorConfig.thresh = 0.3f;
    config.detectorConfig.boxThresh = 0.6f;
    config.detectorConfig.maxCandidates = 1500;
    config.detectorConfig.unclipRatio = 1.5f;
    
    config.recognizerConfig.modelPaths = {
        {3, modelDir + "/server/rec_v5_ratio_3.dxnn"},
        {5, modelDir + "/server/rec_v5_ratio_5.dxnn"},
        {10, modelDir + "/server/rec_v5_ratio_10.dxnn"},
        {15, modelDir + "/server/rec_v5_ratio_15.dxnn"},
        {25, modelDir + "/server/rec_v5_ratio_25.dxnn"},
        {35, modelDir + "/server/rec_v5_ratio_35.dxnn"}
    };
    config.recognizerConfig.dictPath = modelDir + "/ppocrv5_dict.txt";
    config.recognizerConfig.confThreshold = 0.3f;
    config.recognizerConfig.inputHeight = 48;
    
    config.classifierConfig.modelPath = modelDir + "/server/textline_ori.dxnn";
    config.classifierConfig.threshold = 0.9;
    config.classifierConfig.inputWidth = 160;
    config.classifierConfig.inputHeight = 80;
    config.useClassification = true;
    
    // Disable DocPreprocessing for visualization testing (it modifies image geometry)
    config.useDocPreprocessing = false;
    config.docPreprocessingConfig.useOrientation = false;
    config.docPreprocessingConfig.orientationConfig.modelPath = modelDir + "/server/doc_ori_fixed.dxnn";
    config.docPreprocessingConfig.useUnwarping = false;
    config.docPreprocessingConfig.uvdocConfig.modelPath = modelDir + "/server/UVDoc_pruned_p3.dxnn";
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

    // Create output directory for visualization
    std::string outputDir = projectRoot + "/test/pipeline/async_results";
    fs::create_directories(outputDir);
    LOG_INFO("Output directory: {}", outputDir);

    // Pre-load images to memory to measure pure pipeline performance
    std::vector<cv::Mat> images;
    std::vector<std::string> imageNames;
    images.reserve(imageFiles.size());
    imageNames.reserve(imageFiles.size());
    for (const auto& path : imageFiles) {
        cv::Mat img = cv::imread(path);
        if (!img.empty()) {
            images.push_back(img);
            imageNames.push_back(fs::path(path).filename().string());
        }
    }
    LOG_INFO("Loaded {} images", images.size());

    // Start Async Pipeline
    pipeline.start();

    const int NUM_REPEATS = 3;
    int total_tasks = images.size() * NUM_REPEATS;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::atomic<int> completed_count{0};
    
    // Store results for visualization (only last repeat)
    std::map<int, std::vector<ocr::PipelineOCRResult>> savedResults;
    std::mutex resultsMutex;

    // Consumer Thread
    std::thread consumer([&]() {
        std::vector<ocr::PipelineOCRResult> results;
        int64_t id;
        while (completed_count.load() < total_tasks) {
            if (pipeline.getResult(results, id)) {
                completed_count.fetch_add(1);
                
                // Save results from last repeat for visualization
                int imageIdx = id % images.size();
                int repeatIdx = id / images.size();
                
                LOG_INFO("Got result: id={}, imageIdx={}, repeatIdx={}, results={}", 
                         id, imageIdx, repeatIdx, results.size());
                
                if (repeatIdx == NUM_REPEATS - 1) {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    savedResults[imageIdx] = results;
                }
                
                if (completed_count.load() % 10 == 0) {
                    LOG_INFO("Processed {}/{}", completed_count.load(), total_tasks);
                }
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer Loop
    for (int r = 0; r < NUM_REPEATS; ++r) {
        for (size_t i = 0; i < images.size(); ++i) {
            int64_t task_id = r * images.size() + i;
            while (!pipeline.pushTask(images[i], task_id)) {
                // Queue full, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    consumer.join();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    pipeline.stop();

    double total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    double fps = total_tasks / (total_time_ms / 1000.0);

    LOG_INFO("========== Async Performance ==========");
    LOG_INFO("Total Tasks: {} (Images: {}, Repeats: {})", total_tasks, images.size(), NUM_REPEATS);
    LOG_INFO("Total Time: {:.2f} ms", total_time_ms);
    LOG_INFO("Average Time: {:.2f} ms/image", total_time_ms / total_tasks);
    LOG_INFO("FPS: {:.2f}", fps);
    LOG_INFO("=======================================");
    
    // Save visualization results
    LOG_INFO("\nSaving visualization results...");
    std::string fontPath = projectRoot + "/engine/fonts/NotoSansCJK-Regular.ttc";
    int savedCount = 0;
    
    for (size_t i = 0; i < images.size(); ++i) {
        auto it = savedResults.find(i);
        if (it == savedResults.end() || it->second.empty()) continue;
        
        const auto& results = it->second;
        
        // // Debug: check box data
        // if (i == 0 && !results.empty()) {
        //     LOG_INFO("DEBUG: First image results:");
        //     for (size_t k = 0; k < std::min(size_t(3), results.size()); ++k) {
        //         LOG_INFO("  Result[{}]: box.size()={}, text='{}', conf={:.3f}",
        //                  k, results[k].box.size(), 
        //                  results[k].text.empty() ? "<empty>" : results[k].text.substr(0, 20),
        //                  results[k].confidence);
        //         if (results[k].box.size() >= 4) {
        //             LOG_INFO("    box: [{:.1f},{:.1f}] [{:.1f},{:.1f}] [{:.1f},{:.1f}] [{:.1f},{:.1f}]",
        //                      results[k].box[0].x, results[k].box[0].y,
        //                      results[k].box[1].x, results[k].box[1].y,
        //                      results[k].box[2].x, results[k].box[2].y,
        //                      results[k].box[3].x, results[k].box[3].y);
        //         }
        //     }
        // }
        
        // Convert results to TextBox format for visualization
        std::vector<DeepXOCR::TextBox> boxes;
        for (const auto& result : results) {
            DeepXOCR::TextBox box;
            for (int j = 0; j < 4; j++) {
                box.points[j] = result.box[j];
            }
            box.text = result.text;
            box.confidence = result.confidence;
            boxes.push_back(box);
        }
        
        // Draw visualization
        cv::Mat visResult = ocr::Visualizer::drawOCRResultsSideBySide(images[i], boxes, fontPath);
        std::string visPath = outputDir + "/" + imageNames[i];
        cv::imwrite(visPath, visResult);
        savedCount++;
        
        LOG_INFO("Saved: {} ({} boxes)", imageNames[i], results.size());
    }
    
    LOG_INFO("\n✅ Saved {} visualization images to: {}", savedCount, outputDir);

    return 0;
}

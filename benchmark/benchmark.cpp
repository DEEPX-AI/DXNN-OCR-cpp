#include "pipeline/ocr_pipeline.h"
#include "common/logger.hpp"
#include "common/visualizer.h"
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;
using json = nlohmann::json;

int main(int argc, char** argv) {
    // 解析命令行参数：运行次数、模型类型、是否使用UVDoc
    int runsPerImage = 3;
    std::string modelType = "server";  // Default: server models
    bool useUVDoc = false;  // Default: disable UVDoc for accurate benchmark with ground truth labels
    
    if (argc > 1) {
        runsPerImage = std::atoi(argv[1]);
        if (runsPerImage < 1) runsPerImage = 3;
    }
    
    if (argc > 2) {
        modelType = argv[2];
        if (modelType != "server" && modelType != "mobile") {
            LOG_ERROR("Invalid model type: {}. Use 'server' or 'mobile'", modelType);
            return -1;
        }
    }
    
    if (argc > 3) {
        std::string uvdocArg = argv[3];
        if (uvdocArg == "uvdoc" || uvdocArg == "true" || uvdocArg == "1") {
            useUVDoc = true;
        }
    }
    
    LOG_INFO("========================================");
    LOG_INFO("DeepX OCR - Benchmark (Async Mode)");
    LOG_INFO("========================================\n");
    LOG_INFO("Model Type: {}", modelType);
    LOG_INFO("Use UVDoc: {}", useUVDoc ? "Yes" : "No");
    
    std::string projectRoot = PROJECT_ROOT_DIR;
    std::string imagesDir = projectRoot + "/images";
    std::string outputDir = projectRoot + "/benchmark/results_" + modelType;
    std::string visDir = projectRoot + "/benchmark/vis_" + modelType;
    
    fs::create_directories(outputDir);
    fs::create_directories(visDir);
    
    LOG_INFO("📂 Images: {}", imagesDir);
    LOG_INFO("📂 Output: {}", outputDir);
    LOG_INFO("📂 Visualization: {}", visDir);
    LOG_INFO("🔄 Runs per image: {}\n", runsPerImage);
    
    // 配置Pipeline - 根据模型类型配置
    ocr::OCRPipelineConfig config;
    
    // 设置是否使用mobile模型
    bool useMobileModel = (modelType == "mobile");
    config.detectorConfig.useMobileModel = useMobileModel;
    config.recognizerConfig.useMobileModel = useMobileModel;
    
    // 如果使用mobile模型，更新模型路径
    if (useMobileModel) {
        std::string modelRoot = projectRoot + "/engine/model_files/mobile";
        config.detectorConfig.model640Path = modelRoot + "/det_mobile_640.dxnn";
        config.detectorConfig.model960Path = modelRoot + "/det_mobile_960.dxnn";
        
        // 更新Recognition模型路径
        config.recognizerConfig.modelPaths = {
            {3, modelRoot + "/rec_mobile_ratio_3.dxnn"},
            {5, modelRoot + "/rec_mobile_ratio_5.dxnn"},
            {10, modelRoot + "/rec_mobile_ratio_10.dxnn"},
            {15, modelRoot + "/rec_mobile_ratio_15.dxnn"},
            {25, modelRoot + "/rec_mobile_ratio_25.dxnn"},
            {35, modelRoot + "/rec_mobile_ratio_35.dxnn"}
        };
        LOG_INFO("✓ Using mobile models\n");
    } else {
        LOG_INFO("✓ Using server models\n");
    }
    
    // 配置 Document Preprocessing (与 Python demo 一致)
    std::string serverModelRoot = projectRoot + "/engine/model_files/server";
    config.useDocPreprocessing = true;
    config.docPreprocessingConfig.useOrientation = true;
    config.docPreprocessingConfig.orientationConfig.modelPath = serverModelRoot + "/doc_ori_fixed.dxnn";
    config.docPreprocessingConfig.useUnwarping = useUVDoc;  // 使用命令行参数控制
    config.docPreprocessingConfig.uvdocConfig.modelPath = serverModelRoot + "/UVDoc_pruned_p3.dxnn";
    config.docPreprocessingConfig.uvdocConfig.inputWidth = 488;
    config.docPreprocessingConfig.uvdocConfig.inputHeight = 712;
    config.docPreprocessingConfig.uvdocConfig.alignCorners = true;
    
    // 配置 Classification (与 Python demo 一致)
    config.useClassification = true;
    config.classifierConfig.modelPath = serverModelRoot + "/textline_ori.dxnn";
    config.classifierConfig.threshold = 0.9;
    
    // 禁用可视化以提高性能
    config.enableVisualization = false;
    
    // 初始化
    ocr::OCRPipeline pipeline(config);
    if (!pipeline.initialize()) {
        LOG_ERROR("Failed to initialize pipeline");
        return -1;
    }
    
    LOG_INFO("✅ Pipeline initialized\n");
    
    // 获取图片列表
    std::vector<std::string> imageFiles;
    for (const auto& entry : fs::directory_iterator(imagesDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            size_t len = filename.length();
            if ((len > 4 && filename.substr(len - 4) == ".png") ||
                (len > 4 && filename.substr(len - 4) == ".jpg")) {
                imageFiles.push_back(entry.path().string());
            }
        }
    }
    std::sort(imageFiles.begin(), imageFiles.end());
    
    if (imageFiles.empty()) {
        LOG_ERROR("No images found in {}", imagesDir);
        return -1;
    }
    
    LOG_INFO("Found {} images\n", imageFiles.size());
    
    // 预加载所有图片
    std::vector<cv::Mat> images;
    std::vector<std::string> imageNames;
    images.reserve(imageFiles.size());
    imageNames.reserve(imageFiles.size());
    
    for (const auto& imagePath : imageFiles) {
        cv::Mat image = cv::imread(imagePath);
        if (!image.empty()) {
            images.push_back(image);
            imageNames.push_back(fs::path(imagePath).filename().string());
        }
    }
    
    LOG_INFO("Loaded {} images into memory\n", images.size());
    
    // 启动异步 Pipeline
    pipeline.start();
    
    int totalTasks = static_cast<int>(images.size()) * runsPerImage;
    std::atomic<int> completedCount{0};
    
    // 存储每张图片的结果
    std::map<int64_t, std::vector<ocr::PipelineOCRResult>> allResults;
    std::map<int64_t, cv::Mat> processedImages;  // 存储处理后的图像用于可视化
    std::mutex resultsMutex;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 消费者线程：接收结果
    std::thread consumer([&]() {
        while (completedCount.load() < totalTasks) {
            std::vector<ocr::PipelineOCRResult> results;
            cv::Mat processedImage;
            int64_t id;
            if (pipeline.getResult(results, id, &processedImage)) {
                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    // 只保存最后一次运行的结果
                    int imageIdx = id % images.size();
                    int runIdx = id / images.size();
                    
                    LOG_INFO("Got result: id={}, imageIdx={}, runIdx={}, results={}", 
                             id, imageIdx, runIdx, results.size());
                    
                    if (runIdx == runsPerImage - 1) {
                        allResults[imageIdx] = std::move(results);
                        if (!processedImage.empty()) {
                            processedImages[imageIdx] = processedImage.clone();
                        }
                    }
                }
                completedCount.fetch_add(1);
                if (completedCount.load() % 10 == 0) {
                    LOG_INFO("Processed {}/{}", completedCount.load(), totalTasks);
                }
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // 生产者：提交所有任务
    for (int run = 0; run < runsPerImage; ++run) {
        for (size_t i = 0; i < images.size(); ++i) {
            int64_t taskId = run * images.size() + i;
            while (!pipeline.pushTask(images[i], taskId)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    
    consumer.join();
    auto endTime = std::chrono::high_resolution_clock::now();
    
    pipeline.stop();
    
    double totalTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    double avgTimePerImage = totalTimeMs / totalTasks;
    double fps = totalTasks / (totalTimeMs / 1000.0);
    
    LOG_INFO("\n========== Benchmark Results ==========");
    LOG_INFO("Total Tasks: {} (Images: {}, Repeats: {})", totalTasks, images.size(), runsPerImage);
    LOG_INFO("Total Time: {:.2f} ms", totalTimeMs);
    LOG_INFO("Average Time: {:.2f} ms/image", avgTimePerImage);
    LOG_INFO("FPS: {:.2f}", fps);
    LOG_INFO("========================================\n");
    
    // 保存每张图片的详细结果
    std::string fontPath = projectRoot + "/engine/fonts/NotoSansCJK-Regular.ttc";
    int successCount = 0;
    
    for (size_t i = 0; i < images.size(); ++i) {
        auto it = allResults.find(i);
        if (it == allResults.end()) continue;
        
        const auto& results = it->second;
        const std::string& imageName = imageNames[i];
        
        // 构建JSON输出
        json output;
        std::vector<std::string> rec_texts;
        std::vector<float> rec_scores;
        
        for (const auto& result : results) {
            rec_texts.push_back(result.text);
            rec_scores.push_back(result.confidence);
        }
        
        int totalChars = std::accumulate(rec_texts.begin(), rec_texts.end(), 0,
            [](int sum, const std::string& s) { return sum + static_cast<int>(s.length()); });
        
        output["rec_texts"] = rec_texts;
        output["rec_scores"] = rec_scores;
        output["filename"] = imageName;
        output["total_chars"] = totalChars;
        output["runs"] = runsPerImage;
        output["avg_inference_ms"] = avgTimePerImage;
        output["fps"] = fps;
        output["chars_per_second"] = totalChars * 1000.0 / avgTimePerImage;
        
        // 保存JSON
        std::string basePath = outputDir + "/" + 
            imageName.substr(0, imageName.find_last_of('.'));
        std::string jsonPath = basePath + "_result.json";
        std::ofstream jsonFile(jsonPath);
        jsonFile << output.dump(4);
        jsonFile.close();
        
        // 生成可视化
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
        
        // 使用处理后的图像进行可视化（如果有UVDoc预处理），否则使用原图
        cv::Mat imageForVis = processedImages.count(i) ? processedImages[i] : images[i];
        cv::Mat visResult = ocr::Visualizer::drawOCRResultsSideBySide(imageForVis, boxes, fontPath);
        std::string visPath = visDir + "/" + imageName;
        cv::imwrite(visPath, visResult);
        
        successCount++;
    }
    
    LOG_INFO("Completed: {}/{} images", successCount, images.size());
    LOG_INFO("📊 Results saved to: {}", outputDir);
    LOG_INFO("🖼️  Visualizations saved to: {}", visDir);
    LOG_INFO("\n💡 To calculate accuracy and generate full report, run:");
    LOG_INFO("   cd {} && python3 benchmark/run_benchmark.py --no-cpp", projectRoot);
    
    return 0;
}

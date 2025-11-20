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

namespace fs = std::filesystem;
using json = nlohmann::json;

int main(int argc, char** argv) {
    // 解析命令行参数：运行次数
    int runsPerImage = 3;
    if (argc > 1) {
        runsPerImage = std::atoi(argv[1]);
        if (runsPerImage < 1) runsPerImage = 3;
    }
    
    LOG_INFO("========================================");
    LOG_INFO("DeepX OCR - Benchmark");
    LOG_INFO("========================================\n");
    
    std::string projectRoot = PROJECT_ROOT_DIR;
    std::string imagesDir = projectRoot + "/images";
    std::string modelDir = projectRoot + "/engine/model_files/best";
    std::string outputDir = projectRoot + "/benchmark/results";
    std::string visDir = projectRoot + "/benchmark/vis";
    
    fs::create_directories(outputDir);
    fs::create_directories(visDir);
    
    LOG_INFO("📂 Images: %s", imagesDir.c_str());
    LOG_INFO("📂 Output: %s", outputDir.c_str());
    LOG_INFO("📂 Visualization: %s", visDir.c_str());
    LOG_INFO("🔄 Runs per image: %d\n", runsPerImage);
    
    // 配置Pipeline - 完全使用默认配置（所有路径已内置）
    ocr::OCRPipelineConfig config;
    
    // 只需禁用可视化以提高性能
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
    
    LOG_INFO("Found %zu images\n", imageFiles.size());
    
    // 处理每张图片
    int successCount = 0;
    for (size_t i = 0; i < imageFiles.size(); ++i) {
        const std::string& imagePath = imageFiles[i];
        std::string imageName = fs::path(imagePath).filename().string();
        
        LOG_INFO("Processing [%zu/%zu]: %s", i + 1, imageFiles.size(), imageName.c_str());
        
        cv::Mat image = cv::imread(imagePath);
        if (image.empty()) {
            LOG_ERROR("Failed to read image");
            continue;
        }
        
        // 多次运行取平均
        std::vector<double> times;
        std::vector<ocr::PipelineOCRResult> finalResults;
        
        for (int run = 0; run < runsPerImage; ++run) {
            auto start = std::chrono::high_resolution_clock::now();
            std::vector<ocr::PipelineOCRResult> results;
            
            if (!pipeline.process(image, results)) {
                LOG_ERROR("Failed to process");
                break;
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start).count() / 1000.0;
            
            times.push_back(ms);
            if (run == runsPerImage - 1) {
                finalResults = results;
            }
        }
        
        if (times.empty() || times.size() != (size_t)runsPerImage) {
            LOG_ERROR("Failed runs");
            continue;
        }
        
        // 计算统计信息
        double avgTime = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        
        // 保存结果（JSON和TXT格式）
        json output;
        std::vector<std::string> rec_texts;
        std::vector<float> rec_scores;
        
        for (const auto& result : finalResults) {
            rec_texts.push_back(result.text);
            rec_scores.push_back(result.confidence);
        }
        
        int totalChars = std::accumulate(rec_texts.begin(), rec_texts.end(), 0,
            [](int sum, const std::string& s) { return sum + (int)s.length(); });
        
        output["rec_texts"] = rec_texts;
        output["rec_scores"] = rec_scores;
        output["filename"] = imageName;
        output["total_chars"] = totalChars;
        output["runs"] = runsPerImage;
        output["avg_inference_ms"] = avgTime;
        output["fps"] = 1000.0 / avgTime;
        output["chars_per_second"] = totalChars * 1000.0 / avgTime;
        
        // 保存JSON格式
        std::string basePath = outputDir + "/" + 
            imageName.substr(0, imageName.find_last_of('.'));
        std::string jsonPath = basePath + "_result.json";
        std::ofstream jsonFile(jsonPath);
        jsonFile << output.dump(4);
        jsonFile.close();
        
        // 生成可视化图片（左右拼接：左边原图+检测框，右边文字）
        std::string fontPath = projectRoot + "/engine/fonts/NotoSansCJK-Regular.ttc";
        
        // 转换PipelineOCRResult到TextBox格式用于可视化
        std::vector<DeepXOCR::TextBox> boxes;
        for (const auto& result : finalResults) {
            DeepXOCR::TextBox box;
            for (int j = 0; j < 4; j++) {
                box.points[j] = result.box[j];
            }
            box.text = result.text;
            box.confidence = result.confidence;
            boxes.push_back(box);
        }
        
        // 使用预处理后的图片进行可视化，确保框坐标对齐（与test_pipeline一致）
        cv::Mat processedImage = pipeline.getLastProcessedImage();
        cv::Mat visResult = ocr::Visualizer::drawOCRResultsSideBySide(processedImage, boxes, fontPath);
        std::string visPath = visDir + "/" + imageName;
        cv::imwrite(visPath, visResult);
        
        LOG_INFO("  ✓ %d runs, avg: %.2f ms, %zu boxes\n", runsPerImage, avgTime, finalResults.size());
        
        successCount++;
    }
    
    LOG_INFO("========================================");
    LOG_INFO("Completed: %d/%zu images", successCount, imageFiles.size());
    LOG_INFO("========================================\n");
    LOG_INFO("📊 Results saved to: %s", outputDir.c_str());
    LOG_INFO("🖼️  Visualizations saved to: %s", visDir.c_str());
    
    return 0;
}

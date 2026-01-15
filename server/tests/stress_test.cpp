/**
 * @file stress_test.cpp
 * @brief 简单压力测试
 * 
 * 测试 API 参数解析和验证的性能，以及并发处理能力
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include "ocr_handler.h"
#include "json_response.h"

using json = nlohmann::json;
using namespace ocr_server;

// ==================== JSON 解析性能测试 ====================

/**
 * @brief 快速 JSON 解析压力测试
 * 测试高频率 JSON 解析的性能
 */
TEST(StressTest, RapidJsonParsing) {
    const int iterations = 10000;
    
    // 准备测试数据
    json test_json;
    test_json["file"] = "base64_encoded_image_data_here_for_testing_purposes";
    test_json["fileType"] = 1;
    test_json["useDocOrientationClassify"] = true;
    test_json["useDocUnwarping"] = false;
    test_json["useTextlineOrientation"] = true;
    test_json["textDetLimitSideLen"] = 960;
    test_json["textDetLimitType"] = "max";
    test_json["textDetThresh"] = 0.3;
    test_json["textDetBoxThresh"] = 0.6;
    test_json["textDetUnclipRatio"] = 1.5;
    test_json["textRecScoreThresh"] = 0.0;
    test_json["visualize"] = true;
    
    std::string json_str = test_json.dump();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        json parsed = json::parse(json_str);
        OCRRequest req = OCRRequest::FromJson(parsed);
        
        // 简单验证以确保解析正确
        ASSERT_EQ(req.fileType, 1);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_second = iterations * 1000.0 / duration.count();
    
    std::cout << "\n=== RapidJsonParsing Results ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Operations per second: " << ops_per_second << std::endl;
    
    // 性能断言：至少每秒 1000 次解析
    EXPECT_GT(ops_per_second, 1000.0);
}

/**
 * @brief 并发请求验证压力测试
 * 测试多线程同时验证请求参数
 */
TEST(StressTest, ConcurrentRequestValidation) {
    const int num_threads = 8;
    const int iterations_per_thread = 1000;
    
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    auto worker = [&](int thread_id) {
        for (int i = 0; i < iterations_per_thread; ++i) {
            OCRRequest req;
            req.file = "test_file_" + std::to_string(thread_id) + "_" + std::to_string(i);
            req.fileType = 1;
            req.textDetThresh = 0.3;
            req.textDetBoxThresh = 0.6;
            req.textDetUnclipRatio = 1.5;
            req.textRecScoreThresh = 0.0;
            
            std::string error_msg;
            if (req.Validate(error_msg)) {
                success_count.fetch_add(1);
            } else {
                failure_count.fetch_add(1);
            }
        }
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int total_ops = num_threads * iterations_per_thread;
    double ops_per_second = total_ops * 1000.0 / duration.count();
    
    std::cout << "\n=== ConcurrentRequestValidation Results ===" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Total operations: " << total_ops << std::endl;
    std::cout << "Success: " << success_count.load() << std::endl;
    std::cout << "Failures: " << failure_count.load() << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Operations per second: " << ops_per_second << std::endl;
    
    // 验证所有操作都成功
    EXPECT_EQ(success_count.load(), total_ops);
    EXPECT_EQ(failure_count.load(), 0);
    
    // 性能断言：至少每秒 10000 次验证
    EXPECT_GT(ops_per_second, 10000.0);
}

/**
 * @brief 大量参数解析压力测试
 * 测试快速连续解析大量不同参数组合
 */
TEST(StressTest, HighVolumeParameterParsing) {
    const int iterations = 5000;
    
    std::vector<json> test_cases;
    
    // 生成不同的参数组合
    for (int i = 0; i < iterations; ++i) {
        json j;
        j["file"] = "test_file_" + std::to_string(i);
        j["fileType"] = 1;
        j["useDocOrientationClassify"] = (i % 2 == 0);
        j["useDocUnwarping"] = (i % 3 == 0);
        j["useTextlineOrientation"] = (i % 4 == 0);
        j["textDetLimitSideLen"] = 64 + (i % 10) * 100;
        j["textDetLimitType"] = (i % 2 == 0) ? "min" : "max";
        j["textDetThresh"] = 0.1 + (i % 9) * 0.1;
        j["textDetBoxThresh"] = 0.1 + (i % 9) * 0.1;
        j["textDetUnclipRatio"] = 1.0 + (i % 20) * 0.1;
        j["textRecScoreThresh"] = (i % 10) * 0.1;
        j["visualize"] = (i % 5 == 0);
        test_cases.push_back(j);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int valid_count = 0;
    for (const auto& j : test_cases) {
        OCRRequest req = OCRRequest::FromJson(j);
        std::string error_msg;
        if (req.Validate(error_msg)) {
            valid_count++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_second = iterations * 1000.0 / duration.count();
    
    std::cout << "\n=== HighVolumeParameterParsing Results ===" << std::endl;
    std::cout << "Total cases: " << iterations << std::endl;
    std::cout << "Valid cases: " << valid_count << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Operations per second: " << ops_per_second << std::endl;
    
    // 验证大部分都有效（部分可能因为参数范围无效）
    EXPECT_GT(valid_count, iterations * 0.5);
    
    // 性能断言
    EXPECT_GT(ops_per_second, 5000.0);
}

/**
 * @brief JSON 响应构建压力测试
 */
TEST(StressTest, RapidResponseBuilding) {
    const int iterations = 5000;
    
    // 创建测试数据
    std::vector<ocr::PipelineOCRResult> results;
    for (int i = 0; i < 10; ++i) {
        ocr::PipelineOCRResult result;
        result.text = "Test text " + std::to_string(i);
        result.confidence = 0.8f + i * 0.01f;
        result.box = {
            cv::Point2f(i * 10.0f, i * 5.0f),
            cv::Point2f((i + 1) * 10.0f, i * 5.0f),
            cv::Point2f((i + 1) * 10.0f, (i + 1) * 5.0f),
            cv::Point2f(i * 10.0f, (i + 1) * 5.0f)
        };
        results.push_back(result);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        json response = JsonResponseBuilder::BuildSuccessResponse(results, "/static/vis/test.jpg");
        
        // 验证基本结构
        ASSERT_TRUE(response.contains("result"));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_second = iterations * 1000.0 / duration.count();
    
    std::cout << "\n=== RapidResponseBuilding Results ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Results per response: " << results.size() << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Responses per second: " << ops_per_second << std::endl;
    
    // 性能断言
    EXPECT_GT(ops_per_second, 1000.0);
}

/**
 * @brief UUID 生成压力测试
 */
TEST(StressTest, UUIDGeneration) {
    const int iterations = 10000;
    
    std::set<std::string> uuids;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        std::string uuid = JsonResponseBuilder::GenerateUUID();
        uuids.insert(uuid);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_second = iterations * 1000.0 / duration.count();
    
    std::cout << "\n=== UUIDGeneration Results ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Unique UUIDs: " << uuids.size() << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "UUIDs per second: " << ops_per_second << std::endl;
    
    // 验证所有 UUID 都唯一
    EXPECT_EQ(uuids.size(), iterations);
    
    // 性能断言
    EXPECT_GT(ops_per_second, 10000.0);
}

/**
 * @brief 并发 JSON 响应构建测试
 */
TEST(StressTest, ConcurrentResponseBuilding) {
    const int num_threads = 4;
    const int iterations_per_thread = 1000;
    
    std::atomic<int> completed{0};
    
    auto worker = [&](int thread_id) {
        std::vector<ocr::PipelineOCRResult> results;
        ocr::PipelineOCRResult result;
        result.text = "Thread " + std::to_string(thread_id);
        result.confidence = 0.9f;
        result.box = {
            cv::Point2f(0, 0), cv::Point2f(10, 0),
            cv::Point2f(10, 10), cv::Point2f(0, 10)
        };
        results.push_back(result);
        
        for (int i = 0; i < iterations_per_thread; ++i) {
            json response = JsonResponseBuilder::BuildSuccessResponse(results, "");
            ASSERT_TRUE(response.contains("logId"));
            completed.fetch_add(1);
        }
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int total_ops = num_threads * iterations_per_thread;
    double ops_per_second = total_ops * 1000.0 / duration.count();
    
    std::cout << "\n=== ConcurrentResponseBuilding Results ===" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Total operations: " << total_ops << std::endl;
    std::cout << "Completed: " << completed.load() << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Operations per second: " << ops_per_second << std::endl;
    
    EXPECT_EQ(completed.load(), total_ops);
    EXPECT_GT(ops_per_second, 5000.0);
}

/**
 * @brief 错误响应构建压力测试
 */
TEST(StressTest, ErrorResponseBuilding) {
    const int iterations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        int error_code = (i % 4 == 0) ? ErrorCode::INVALID_PARAMETER :
                        (i % 4 == 1) ? ErrorCode::UNAUTHORIZED :
                        (i % 4 == 2) ? ErrorCode::INTERNAL_ERROR :
                        ErrorCode::SERVICE_UNAVAILABLE;
        
        json response = JsonResponseBuilder::BuildErrorResponse(
            error_code, "Error message " + std::to_string(i));
        
        ASSERT_TRUE(response.contains("errorCode"));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_second = iterations * 1000.0 / duration.count();
    
    std::cout << "\n=== ErrorResponseBuilding Results ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Responses per second: " << ops_per_second << std::endl;
    
    EXPECT_GT(ops_per_second, 10000.0);
}

// ==================== 内存压力测试 ====================

/**
 * @brief 大量请求对象创建和销毁
 * 检测潜在的内存泄漏
 */
TEST(StressTest, MemoryStress) {
    const int iterations = 10000;
    
    for (int i = 0; i < iterations; ++i) {
        // 创建请求
        json j;
        j["file"] = std::string(1000, 'x');  // 较大的字符串
        j["fileType"] = 1;
        j["useDocOrientationClassify"] = true;
        j["textDetThresh"] = 0.5;
        
        OCRRequest req = OCRRequest::FromJson(j);
        
        // 创建响应
        std::vector<ocr::PipelineOCRResult> results;
        for (int r = 0; r < 20; ++r) {
            ocr::PipelineOCRResult result;
            result.text = std::string(100, 'y');
            result.confidence = 0.9f;
            result.box = {
                cv::Point2f(0, 0), cv::Point2f(100, 0),
                cv::Point2f(100, 100), cv::Point2f(0, 100)
            };
            results.push_back(result);
        }
        
        json response = JsonResponseBuilder::BuildSuccessResponse(results, "/test/url");
        
        // 对象在这里被销毁
    }
    
    // 如果能运行到这里，说明没有内存泄漏导致的崩溃
    SUCCEED();
}

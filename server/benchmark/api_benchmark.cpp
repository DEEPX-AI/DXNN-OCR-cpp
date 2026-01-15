/**
 * @file api_benchmark.cpp
 * @brief OCR API Server 基准测试工具
 * 
 * 对 OCR HTTP API 进行性能测试，测量响应时间、吞吐量等指标
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <getopt.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==================== 配置结构 ====================

struct BenchmarkConfig {
    std::string server_url = "http://localhost:8080/ocr";
    std::string token = "test_token";
    int total_requests = 100;
    int concurrency = 8;
    std::string images_dir = "";  // 可选：从目录加载图片
    std::string output_file = "api_benchmark_results.json";
    bool verbose = false;
};

// ==================== 结果统计 ====================

struct RequestResult {
    bool success;
    int http_code;
    double latency_ms;
    std::string error_msg;
};

struct BenchmarkResults {
    int total_requests;
    int successful_requests;
    int failed_requests;
    double total_time_ms;
    double avg_latency_ms;
    double min_latency_ms;
    double max_latency_ms;
    double p50_latency_ms;
    double p90_latency_ms;
    double p99_latency_ms;
    double qps;
    double success_rate;
    std::vector<RequestResult> all_results;
};

// ==================== CURL 回调 ====================

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ==================== Base64 编码（简单实现） ====================

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::vector<unsigned char>& data) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = data.size();
    const unsigned char* bytes_to_encode = data.data();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}

// ==================== 图片加载 ====================

std::string loadImageAsBase64(const std::string& image_path) {
    std::ifstream file(image_path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::vector<unsigned char> buffer(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    
    return base64_encode(buffer);
}

std::vector<std::string> loadImagesFromDirectory(const std::string& dir_path) {
    std::vector<std::string> base64_images;
    
    if (!fs::exists(dir_path)) {
        std::cerr << "Directory not found: " << dir_path << std::endl;
        return base64_images;
    }
    
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;
        
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
            std::string base64 = loadImageAsBase64(entry.path().string());
            if (!base64.empty()) {
                base64_images.push_back(base64);
                std::cout << "Loaded: " << entry.path().filename().string() << std::endl;
            }
        }
    }
    
    return base64_images;
}

// ==================== HTTP 请求 ====================

RequestResult sendOCRRequest(
    const std::string& url,
    const std::string& token,
    const std::string& image_base64,
    bool verbose) {
    
    RequestResult result;
    result.success = false;
    result.http_code = 0;
    result.latency_ms = 0;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error_msg = "Failed to initialize CURL";
        return result;
    }
    
    // 构建请求 JSON
    json request_json;
    request_json["file"] = image_base64;
    request_json["fileType"] = 1;
    request_json["useDocOrientationClassify"] = true;
    request_json["useDocUnwarping"] = false;
    request_json["textDetThresh"] = 0.3;
    request_json["textDetBoxThresh"] = 0.6;
    request_json["textDetUnclipRatio"] = 1.5;
    request_json["textRecScoreThresh"] = 0.0;
    request_json["visualize"] = false;
    
    std::string request_body = request_json.dump();
    std::string response_body;
    
    // 设置 CURL 选项
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: token " + token;
    headers = curl_slist_append(headers, auth_header.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    
    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    
    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();
    result.latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // 获取 HTTP 状态码
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.http_code);
    
    if (res != CURLE_OK) {
        result.error_msg = curl_easy_strerror(res);
    } else {
        try {
            json response = json::parse(response_body);
            if (response.contains("errorCode") && response["errorCode"] == 0) {
                result.success = true;
            } else {
                result.error_msg = response.value("errorMsg", "Unknown error");
            }
        } catch (const json::exception& e) {
            result.error_msg = std::string("JSON parse error: ") + e.what();
        }
    }
    
    if (verbose) {
        std::cout << "Request completed: HTTP " << result.http_code 
                  << ", " << result.latency_ms << " ms"
                  << (result.success ? " [OK]" : " [FAIL: " + result.error_msg + "]")
                  << std::endl;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return result;
}

// ==================== 基准测试执行 ====================

BenchmarkResults runBenchmark(
    const BenchmarkConfig& config,
    const std::vector<std::string>& images) {
    
    BenchmarkResults results;
    results.total_requests = config.total_requests;
    results.successful_requests = 0;
    results.failed_requests = 0;
    
    std::mutex results_mutex;
    std::atomic<int> completed{0};
    std::atomic<int> request_index{0};
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Starting API Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server URL: " << config.server_url << std::endl;
    std::cout << "Total Requests: " << config.total_requests << std::endl;
    std::cout << "Concurrency: " << config.concurrency << std::endl;
    std::cout << "Images loaded: " << images.size() << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    auto worker = [&]() {
        while (true) {
            int idx = request_index.fetch_add(1);
            if (idx >= config.total_requests) break;
            
            // 选择图片（循环使用）
            const std::string& image = images[idx % images.size()];
            
            // 发送请求
            RequestResult req_result = sendOCRRequest(
                config.server_url, config.token, image, config.verbose);
            
            // 记录结果
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.all_results.push_back(req_result);
                if (req_result.success) {
                    results.successful_requests++;
                } else {
                    results.failed_requests++;
                }
            }
            
            int done = completed.fetch_add(1) + 1;
            if (done % 10 == 0 || done == config.total_requests) {
                std::cout << "\rProgress: " << done << "/" << config.total_requests 
                          << " (" << (done * 100 / config.total_requests) << "%)" << std::flush;
            }
        }
    };
    
    // 开始计时
    auto start = std::chrono::high_resolution_clock::now();
    
    // 启动工作线程
    std::vector<std::thread> threads;
    for (int i = 0; i < config.concurrency; ++i) {
        threads.emplace_back(worker);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 结束计时
    auto end = std::chrono::high_resolution_clock::now();
    results.total_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << std::endl;
    
    // 计算统计数据
    std::vector<double> latencies;
    for (const auto& r : results.all_results) {
        latencies.push_back(r.latency_ms);
    }
    
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        
        results.min_latency_ms = latencies.front();
        results.max_latency_ms = latencies.back();
        results.avg_latency_ms = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        
        // 百分位数
        results.p50_latency_ms = latencies[latencies.size() * 50 / 100];
        results.p90_latency_ms = latencies[latencies.size() * 90 / 100];
        results.p99_latency_ms = latencies[std::min(latencies.size() - 1, latencies.size() * 99 / 100)];
    }
    
    results.qps = config.total_requests * 1000.0 / results.total_time_ms;
    results.success_rate = results.successful_requests * 100.0 / config.total_requests;
    
    return results;
}

// ==================== 结果输出 ====================

void printResults(const BenchmarkResults& results) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Benchmark Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total Requests:    " << results.total_requests << std::endl;
    std::cout << "Successful:        " << results.successful_requests << std::endl;
    std::cout << "Failed:            " << results.failed_requests << std::endl;
    std::cout << "Success Rate:      " << results.success_rate << "%" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total Time:        " << results.total_time_ms << " ms" << std::endl;
    std::cout << "QPS:               " << results.qps << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Latency (ms):" << std::endl;
    std::cout << "  Min:             " << results.min_latency_ms << std::endl;
    std::cout << "  Max:             " << results.max_latency_ms << std::endl;
    std::cout << "  Avg:             " << results.avg_latency_ms << std::endl;
    std::cout << "  P50:             " << results.p50_latency_ms << std::endl;
    std::cout << "  P90:             " << results.p90_latency_ms << std::endl;
    std::cout << "  P99:             " << results.p99_latency_ms << std::endl;
    std::cout << "========================================" << std::endl;
}

void saveResults(const BenchmarkResults& results, const std::string& output_file) {
    json output;
    output["total_requests"] = results.total_requests;
    output["successful_requests"] = results.successful_requests;
    output["failed_requests"] = results.failed_requests;
    output["success_rate"] = results.success_rate;
    output["total_time_ms"] = results.total_time_ms;
    output["qps"] = results.qps;
    output["avg_latency_ms"] = results.avg_latency_ms;
    output["min_latency_ms"] = results.min_latency_ms;
    output["max_latency_ms"] = results.max_latency_ms;
    output["p50_latency_ms"] = results.p50_latency_ms;
    output["p90_latency_ms"] = results.p90_latency_ms;
    output["p99_latency_ms"] = results.p99_latency_ms;
    
    std::ofstream file(output_file);
    file << output.dump(4);
    file.close();
    
    std::cout << "\nResults saved to: " << output_file << std::endl;
}

// ==================== 主函数 ====================

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  -u, --url <url>          Server URL (default: http://localhost:8080/ocr)" << std::endl;
    std::cout << "  -t, --token <token>      Authorization token (default: test_token)" << std::endl;
    std::cout << "  -n, --requests <num>     Total number of requests (default: 100)" << std::endl;
    std::cout << "  -c, --concurrency <num>  Number of concurrent workers (default: 8)" << std::endl;
    std::cout << "  -i, --images <dir>       Directory containing test images" << std::endl;
    std::cout << "  -o, --output <file>      Output JSON file (default: api_benchmark_results.json)" << std::endl;
    std::cout << "  -v, --verbose            Verbose output" << std::endl;
    std::cout << "  -h, --help               Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    BenchmarkConfig config;
    
    static struct option long_options[] = {
        {"url",         required_argument, 0, 'u'},
        {"token",       required_argument, 0, 't'},
        {"requests",    required_argument, 0, 'n'},
        {"concurrency", required_argument, 0, 'c'},
        {"images",      required_argument, 0, 'i'},
        {"output",      required_argument, 0, 'o'},
        {"verbose",     no_argument,       0, 'v'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "u:t:n:c:i:o:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'u':
                config.server_url = optarg;
                break;
            case 't':
                config.token = optarg;
                break;
            case 'n':
                config.total_requests = std::stoi(optarg);
                break;
            case 'c':
                config.concurrency = std::stoi(optarg);
                break;
            case 'i':
                config.images_dir = optarg;
                break;
            case 'o':
                config.output_file = optarg;
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // 初始化 CURL
    curl_global_init(CURL_GLOBAL_ALL);
    
    // 加载测试图片
    std::vector<std::string> images;
    
    if (!config.images_dir.empty()) {
        images = loadImagesFromDirectory(config.images_dir);
    }
    
    // 如果没有图片，使用一个简单的测试图片（1x1 红色 PNG）
    if (images.empty()) {
        std::cout << "No images loaded, using default test image" << std::endl;
        images.push_back(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg=="
        );
    }
    
    // 运行基准测试
    BenchmarkResults results = runBenchmark(config, images);
    
    // 输出结果
    printResults(results);
    saveResults(results, config.output_file);
    
    // 清理 CURL
    curl_global_cleanup();
    
    return 0;
}

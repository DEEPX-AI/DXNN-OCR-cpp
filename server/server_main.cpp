#include "ocr_handler.h"
#include "json_response.h"
#include "common/logger.hpp"
#include <crow.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <memory>
#include <string>
#include <cstring>

using json = nlohmann::json;
using namespace ocr_server;

/**
 * @brief 安全解析整数参数
 * @param arg 输入字符串
 * @param value 输出的整数值
 * @param name 参数名称（用于错误提示）
 * @param min_val 最小允许值
 * @param max_val 最大允许值
 * @return 解析成功返回 true，失败返回 false
 */
static bool parseIntArg(const char* arg, int& value, const char* name,
                        int min_val = INT_MIN, int max_val = INT_MAX) {
    // 检查空指针或空字符串
    if (arg == nullptr || arg[0] == '\0') {
        std::cerr << "Error: " << name << " value can't be empty" << std::endl;
        return false;
    }

    try {
        size_t pos = 0;
        int parsed = std::stoi(arg, &pos);

        // 检查是否完全转换：防止 "123abc" 字母数字混合输入情况
        if (pos != strlen(arg)) {
            std::cerr << "Error: " << name << " value: '" << arg 
                      << "' (contains non-digit characters)" << std::endl;
            return false;
        }

        // 范围检查
        if (parsed < min_val || parsed > max_val) {
            std::cerr << "Error: " << name << " must be in range [" 
                      << min_val << ", " << max_val << "], got: " << parsed << std::endl;
            return false;
        }

        value = parsed;
        return true;
    } catch (const std::invalid_argument&) {
        std::cerr << "Error: Invalid " << name << " value: '" << arg
                  << "' (not a valid integer)" << std::endl;
        return false;
    } catch (const std::out_of_range&) {
        std::cerr << "Error: " << name << " value out of range: '" << arg << "'/n" << std::endl;
        return false;
    }
}

/**
 * @brief 加载OCR Pipeline配置
 * @param useMobileModel 是否使用 mobile 模型
 */
ocr::OCRPipelineConfig LoadPipelineConfig(bool useMobileModel) {
    ocr::OCRPipelineConfig config;
    
    // 设置模型类型
    config.detectorConfig.useMobileModel = useMobileModel;
    config.recognizerConfig.useMobileModel = useMobileModel;
    
    // Document Preprocessing配置
    config.docPreprocessingConfig.useOrientation = true;
    config.docPreprocessingConfig.useUnwarping = true;
    
    // Pipeline配置
    config.useDocPreprocessing = true;
    config.useClassification = true;
    config.enableVisualization = true;
    config.sortResults = true;
    
    if (useMobileModel) {
        LOG_INFO("Using MOBILE models");
    } else {
        LOG_INFO("Using SERVER models");
    }
    
    return config;
}

/**
 * @brief 认证中间件（简单的Token验证）
 */
struct AuthMiddleware : crow::ILocalMiddleware {
    struct context {};
    
    void before_handle(crow::request& req, crow::response& res, context&) {
        // 获取Authorization header
        auto auth_header = req.get_header_value("Authorization");
        
        // 简单验证：检查是否以"token "开头
        if (auth_header.empty() || auth_header.find("token ") != 0) {
            LOG_WARN("Unauthorized request from {}", req.remote_ip_address);
            
            json error_response = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::UNAUTHORIZED, "Missing or invalid Authorization token");
            
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.write(error_response.dump());
            res.end();
            return;
        }
        
        // 提取token（可以在这里进行更复杂的验证）
        std::string token = auth_header.substr(6);  // 跳过"token "
        
        // TODO: 在这里可以添加真实的token验证逻辑
        // 例如：查询数据库、验证JWT等
        
        LOG_INFO("Authenticated request from {} with token: {}...", 
                 req.remote_ip_address, token.substr(0, 8));
    }
    
    void after_handle(crow::request&, crow::response&, context&) {
        // 后处理（可选）
    }
};

int main(int argc, char* argv[]) {
    // 默认参数
    int port = 8080;
    int threads = 4;
    std::string vis_dir = "output/vis";
    std::string model_type = "server";
    std::string log_dir = "logs";  // 默认日志目录
    
    // 定义长选项
    static struct option long_options[] = {
        {"port",     required_argument, 0, 'p'},
        {"threads",  required_argument, 0, 't'},
        {"vis-dir",  required_argument, 0, 'v'},
        {"model",    required_argument, 0, 'm'},
        {"log-dir",  required_argument, 0, 'l'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    // 解析命令行参数
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "p:t:v:m:l:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'p':
                // 端口范围: 1-65535 (标准TCP/UDP端口范围)
                if (!parseIntArg(optarg, port, "port", 1, 65535)) {
                    return 1;
                }
                break;
            case 't':
                // 线程数范围: 1-256 (合理的线程数限制)
                if (!parseIntArg(optarg, threads, "threads", 1, 256)) {
                    return 1;
                }
                break;
            case 'v':
                vis_dir = optarg;
                break;
            case 'm':
                model_type = optarg;
                if (model_type != "server" && model_type != "mobile") {
                    std::cerr << "Error: model must be 'server' or 'mobile'\n";
                    return 1;
                }
                break;
            case 'l':
                log_dir = optarg;
                break;
            case 'h':
                std::cout << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  -p, --port <port>        Server port (default: 8080)\n"
                          << "  -t, --threads <num>      Number of threads (default: 4)\n"
                          << "  -v, --vis-dir <path>     Visualization output directory (default: output/vis)\n"
                          << "  -m, --model <type>       Model type: 'server' or 'mobile' (default: server)\n"
                          << "  -l, --log-dir <path>     Log directory (default: logs)\n"
                          << "  -h, --help               Show this help message\n";
                return 0;
            default:
                std::cerr << "Use -h or --help for usage information\n";
                return 1;
        }
    }
    
    // 创建日志目录并初始化 Logger
    std::filesystem::create_directories(log_dir);
    DeepXOCR::LoggerConfig logConfig;
    logConfig.logDir = log_dir;
    try {
        DeepXOCR::InitLogger(logConfig);
    } catch (const spdlog::spdlog_ex& ex) {
        throw;
    }
    
    // 初始化日志
    LOG_INFO("========== DeepX OCR Server Starting ==========");
    LOG_INFO("Log directory: {}", log_dir);
    
    // 创建可视化输出目录
    std::filesystem::create_directories(vis_dir);
    LOG_INFO("Visualization output directory: {}", vis_dir);
    
    // 加载OCR Pipeline配置
    LOG_INFO("Loading OCR Pipeline configuration...");
    bool useMobileModel = (model_type == "mobile");
    auto pipeline_config = LoadPipelineConfig(useMobileModel);
    pipeline_config.Show();
    
    // 创建OCR Handler
    LOG_INFO("Initializing OCR Handler...");
    auto ocr_handler = std::make_shared<OCRHandler>(
        pipeline_config, vis_dir, "/static/vis");
    
    // 创建Crow应用（带认证中间件）
    crow::App<AuthMiddleware> app;
    
    // 健康检查接口
    CROW_ROUTE(app, "/health")
    ([]() {
        json response;
        response["status"] = "healthy";
        response["service"] = "DeepX OCR Server";
        response["version"] = "1.0.0";
        return crow::response(response.dump());
    });
    
    // OCR识别接口
    CROW_ROUTE(app, "/ocr").methods(crow::HTTPMethod::POST)
    ([ocr_handler](const crow::request& req) {
        LOG_INFO("Received OCR request from {}", req.remote_ip_address);
        
        try {
            // 解析JSON请求
            json request_json = json::parse(req.body);
            
            // 转换为OCRRequest对象
            auto ocr_request = OCRRequest::FromJson(request_json);
            
            // 处理请求
            json response_json;
            int status_code = ocr_handler->HandleRequest(ocr_request, response_json);
            
            // 返回响应
            crow::response res(status_code, response_json.dump());
            res.set_header("Content-Type", "application/json");
            return res;
            
        } catch (const json::exception& e) {
            LOG_ERROR("JSON parse error: {}", e.what());
            json error_response = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INVALID_PARAMETER, 
                std::string("Invalid JSON format: ") + e.what());
            
            crow::response res(400, error_response.dump());
            res.set_header("Content-Type", "application/json");
            return res;
            
        } catch (const std::exception& e) {
            LOG_ERROR("Unexpected error: {}", e.what());
            json error_response = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INTERNAL_ERROR,
                std::string("Internal server error: ") + e.what());
            
            crow::response res(500, error_response.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        } catch (...) {
            LOG_ERROR("Unknown exception occurred");
            json error_response = JsonResponseBuilder::BuildErrorResponse(
                ErrorCode::INTERNAL_ERROR,
                "Internal server error: unknown exception");
            
            crow::response res(500, error_response.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }
    });
    
    // 静态文件服务（用于访问可视化图片）
    CROW_ROUTE(app, "/static/vis/<path>")
    ([vis_dir](crow::response& res, std::string filename) {
        std::string filepath = vis_dir + "/" + filename;
        
        // 使用 Crow 内置方法：
        // 1. 自动进行路径安全清理（防止路径穿越攻击）
        // 2. 自动根据文件扩展名设置 Content-Type
        // 3. 自动设置 Content-Length
        // 4. 文件不存在时自动返回 404
        LOG_DEBUG("Serving static file: {}", filepath);
        res.set_static_file_info(filepath);
        res.end();
    });
    
    // 启动服务器
    LOG_INFO("Starting server on port {} with {} threads...", port, threads);
    LOG_INFO("Endpoints:");
    LOG_INFO("  - POST   /ocr           (OCR Recognition)");
    LOG_INFO("  - GET    /health        (Health Check)");
    LOG_INFO("  - GET    /static/vis/*  (Visualization Images)");
    LOG_INFO("===============================================");
    
    app.port(port)
       .concurrency(threads)
       .run();
    
    return 0;
}

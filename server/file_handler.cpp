#include "file_handler.h"
#include "common/logger.hpp"
#include "base64.h"  // cpp-base64库
#include <curl/curl.h>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace ocr_server {

// 带大小限制的下载缓冲区结构
struct DownloadBuffer {
    std::vector<uchar> data;
    size_t max_size;
    bool exceeded = false;
};

std::string FileHandler::Base64Decode(const std::string& encoded_string) {
    return base64_decode(encoded_string, false);
}

bool FileHandler::DecodeBase64Image(const std::string& base64_str, cv::Mat& image) {
    try {
        // 移除可能的 "data:image/...;base64," 前缀
        std::string clean_base64 = base64_str;
        size_t comma_pos = base64_str.find(',');
        if (comma_pos != std::string::npos) {
            clean_base64 = base64_str.substr(comma_pos + 1);
        }
        
        // Base64解码
        std::string decoded = Base64Decode(clean_base64);
        if (decoded.empty()) {
            LOG_ERROR("Base64 decode failed: empty result");
            return false;
        }
        
        // 将解码后的数据转换为OpenCV Mat
        std::vector<uchar> data(decoded.begin(), decoded.end());
        image = cv::imdecode(data, cv::IMREAD_COLOR);
        
        if (image.empty()) {
            LOG_ERROR("Failed to decode image from Base64 data");
            return false;
        }
        
        LOG_INFO("Successfully decoded Base64 image: {}x{}", image.cols, image.rows);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in DecodeBase64Image: {}", e.what());
        return false;
    }
}

size_t FileHandler::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    DownloadBuffer* buffer = static_cast<DownloadBuffer*>(userp);

    // 检查超过大小的限制
    if (buffer->data.size() + total_size > buffer->max_size) {
        buffer->exceeded = true;
        return 0; // 返回 0 表示中止下载
    }

    buffer->data.insert(buffer->data.end(),
                        static_cast<uchar*>(contents),
                        static_cast<uchar*>(contents) + total_size);
    return total_size;
}

bool FileHandler::DownloadImageFromURL(const std::string& url, cv::Mat& image, 
                                        const DownloadConfig& config) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return false;
    }

    DownloadBuffer buffer;
    buffer.max_size = config.maxDownloadSize;

    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // ===== SSL 验证配置（修复安全漏洞）=====
    if (config.verifySSL) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);  // 验证证书
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);  // 验证主机名
    } else {
        LOG_WARN("SSL verification disabled for URL download - NOT recommended for production!");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // 执行下载
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    // 检查下载大小是否超限
    if (buffer.exceeded) {
        LOG_ERROR("Download size exceeded limit ({} MB)", config.maxDownloadSize / (1024 * 1024));
        return false;
    }

    if (res != CURLE_OK) {
        // SSL 验证失败时给出更明确的提示
        if (res == CURLE_SSL_CACERT || res == CURLE_SSL_CERTPROBLEM) {
            LOG_ERROR("SSL certificate verification failed for URL: {}", url);
            LOG_ERROR("If this is a trusted internal server, consider disabling SSL verification in config");
        } else {
            LOG_ERROR("CURL download failed: {} ({})", curl_easy_strerror(res), static_cast<int>(res));
        }
        return false;
    }

    if (http_code != 200) {
        LOG_ERROR("HTTP request failed with code: {}", http_code);
        return false;
    }

    if (buffer.data.empty()) {
        LOG_ERROR("Downloaded empty data from URL: {}", url);
        return false;
    }

    // 解码图像
    image = cv::imdecode(buffer.data, cv::IMREAD_COLOR);
    if (image.empty()) {
        LOG_ERROR("Failed to decode downloaded image from URL: {}", url);
        return false;
    }

    LOG_INFO("Successfully downloaded image from URL: {}x{} ({} bytes)", 
                image.cols, image.rows, buffer.data.size());
    return true;
}

std::string FileHandler::SaveVisualizationImage(const cv::Mat& image, const std::string& output_dir) {
    try {
        // 确保输出目录存在
        std::filesystem::create_directories(output_dir);
        
        // 生成唯一文件名（使用时间戳）
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::ostringstream filename;
        filename << "ocr_vis_" << timestamp << ".jpg";
        
        std::string filepath = output_dir + "/" + filename.str();
        
        // 保存图像
        std::vector<int> compression_params = {cv::IMWRITE_JPEG_QUALITY, 90};
        if (!cv::imwrite(filepath, image, compression_params)) {
            LOG_ERROR("Failed to save visualization image: {}", filepath);
            return "";
        }
        
        LOG_INFO("Saved visualization image: {}", filepath);
        return filename.str();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in SaveVisualizationImage: {}", e.what());
        return "";
    }
}

} // namespace ocr_server

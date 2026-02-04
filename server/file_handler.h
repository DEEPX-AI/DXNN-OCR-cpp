#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace ocr_server {

/**
 * @brief 下载配置结构
 */
 struct DownloadConfig {
    bool verifySSL = true;                          // 是否验证 SSL 证书（默认启用）
    int timeoutSeconds = 10;                        // 下载超时时间（秒）
    size_t maxDownloadSize = 50 * 1024 * 1024;      // 最大下载大小（50MB）
};

/**
 * @brief 文件处理工具类：Base64解码和URL下载
 */
class FileHandler {
public:
    /**
     * @brief 从Base64字符串解码图像
     * @param base64_str Base64编码的图像字符串
     * @param image 输出的图像
     * @return 成功返回true
     */
    static bool DecodeBase64Image(const std::string& base64_str, cv::Mat& image);
    
    /**
     * @brief 从URL下载图像
     * @param url 图像URL地址
     * @param image 输出的图像
     * @param config 下载配置（可选）
     * @return 成功返回true
     */
     static bool DownloadImageFromURL(const std::string& url, cv::Mat& image, 
        const DownloadConfig& config = {});

    /**
     * @brief 保存可视化图像到临时目录
     * @param image 要保存的图像
     * @param output_dir 输出目录
     * @return 生成的文件名（不含路径）
     */
     static std::string SaveVisualizationImage(const cv::Mat& image, 
        const std::string& output_dir = "/tmp/ocr_vis");

private:
    // Base64解码辅助函数
    static std::string Base64Decode(const std::string& encoded_string);
    
    // HTTP下载回调函数 (带大小限制)
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

} // namespace ocr_server

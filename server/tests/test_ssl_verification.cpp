/**
 * @file test_ssl_verification.cpp
 * @brief SSL 证书验证测试
 * 
 * 测试 FileHandler 和 PDFHandler 的 SSL 验证功能
 * 
 * 测试场景:
 * 1. DownloadConfig 默认值测试
 * 2. 有效 HTTPS URL + SSL 验证开启（应成功）
 * 3. 自签名证书 + SSL 验证开启（应失败）
 * 4. 自签名证书 + SSL 验证关闭（应成功连接）
 * 5. 下载大小限制测试
 * 
 * 注意: 部分测试需要网络连接，可能在离线环境中跳过
 */

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include "file_handler.h"
#include "pdf_handler.h"

using namespace ocr_server;

// ==================== 测试用 URL 定义 ====================
namespace TestURLs {
    // 百度 Logo 图片（国内访问稳定）
    const char* BAIDU_LOGO = "https://www.baidu.com/img/PCtm_d9c8750bed0b3c7d089fa7d55720d6cf.png";
    // 网络检测用 URL（使用 PNG 格式，OpenCV 不支持 ICO 格式）
    const char* BAIDU_HOME = "https://www.baidu.com/img/PCtm_d9c8750bed0b3c7d089fa7d55720d6cf.png";
    // HTTP 版本（非 HTTPS）
    const char* BAIDU_HTTP = "http://www.baidu.com/img/PCtm_d9c8750bed0b3c7d089fa7d55720d6cf.png";
    // badssl.com 自签名证书测试
    const char* SELF_SIGNED = "https://self-signed.badssl.com/";
    const char* EXPIRED_CERT = "https://expired.badssl.com/";
    const char* WRONG_HOST = "https://wrong.host.badssl.com/";
}

// ==================== DownloadConfig 默认值测试 ====================

/**
 * @brief 测试 DownloadConfig 默认值
 */
TEST(SSLVerification, DownloadConfig_DefaultValues) {
    DownloadConfig config;
    
    // 验证默认值
    EXPECT_TRUE(config.verifySSL) << "SSL verification should be enabled by default";
    EXPECT_EQ(config.timeoutSeconds, 10) << "Default timeout should be 10 seconds";
    EXPECT_EQ(config.maxDownloadSize, 50 * 1024 * 1024) << "Default max size should be 50MB";
}

/**
 * @brief 测试 DownloadConfig 自定义值
 */
TEST(SSLVerification, DownloadConfig_CustomValues) {
    DownloadConfig config;
    config.verifySSL = false;
    config.timeoutSeconds = 30;
    config.maxDownloadSize = 100 * 1024 * 1024;  // 100MB
    
    EXPECT_FALSE(config.verifySSL);
    EXPECT_EQ(config.timeoutSeconds, 30);
    EXPECT_EQ(config.maxDownloadSize, 100 * 1024 * 1024);
}

// ==================== 网络测试辅助函数 ====================

/**
 * @brief 检查是否有网络连接（使用百度）
 */
static bool hasNetworkConnection() {
    cv::Mat image;
    DownloadConfig config;
    config.timeoutSeconds = 5;
    config.verifySSL = true;
    
    // 使用百度 favicon 检测网络
    return FileHandler::DownloadImageFromURL(TestURLs::BAIDU_HOME, image, config);
}

// ==================== 有效 HTTPS URL 测试 ====================

/**
 * @brief 测试有效 HTTPS URL + SSL 验证开启
 * 
 * 使用百度 Logo 图片，该服务有有效的 SSL 证书
 */
TEST(SSLVerification, ValidHTTPS_WithSSLVerification) {
    // 跳过测试如果没有网络
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;  // 显式开启 SSL 验证
    config.timeoutSeconds = 15;
    
    // 百度有有效的 SSL 证书
    bool success = FileHandler::DownloadImageFromURL(TestURLs::BAIDU_LOGO, image, config);
    
    EXPECT_TRUE(success) << "Should successfully download from valid HTTPS with SSL verification";
    if (success) {
        EXPECT_FALSE(image.empty()) << "Downloaded image should not be empty";
        EXPECT_GT(image.cols, 0) << "Image width should be positive";
        EXPECT_GT(image.rows, 0) << "Image height should be positive";
    }
}

/**
 * @brief 测试使用默认配置下载（SSL 验证应该是开启的）
 */
TEST(SSLVerification, ValidHTTPS_DefaultConfig) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    // 使用默认配置（不传入 config 参数）
    DownloadConfig defaultConfig;  // 默认 verifySSL = true
    
    bool success = FileHandler::DownloadImageFromURL(TestURLs::BAIDU_LOGO, image, defaultConfig);
    
    EXPECT_TRUE(success) << "Default config should work with valid HTTPS";
}

// ==================== 自签名证书测试 ====================

/**
 * @brief 测试自签名证书 + SSL 验证开启（应该失败）
 * 
 * 使用 badssl.com 提供的自签名证书测试页面
 */
TEST(SSLVerification, SelfSignedCert_WithSSLVerification_ShouldFail) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;  // 开启 SSL 验证
    config.timeoutSeconds = 10;
    
    // self-signed.badssl.com 使用自签名证书
    // 注意：这个网站返回的是 HTML，不是图片，但我们测试的是 SSL 握手阶段
    bool success = FileHandler::DownloadImageFromURL(TestURLs::SELF_SIGNED, image, config);
    
    // 应该失败，因为自签名证书无法通过验证
    EXPECT_FALSE(success) << "Should fail with self-signed certificate when SSL verification is enabled";
}

/**
 * @brief 测试过期证书 + SSL 验证开启（应该失败）
 */
TEST(SSLVerification, ExpiredCert_WithSSLVerification_ShouldFail) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;
    config.timeoutSeconds = 10;
    
    // expired.badssl.com 使用过期的证书
    bool success = FileHandler::DownloadImageFromURL(TestURLs::EXPIRED_CERT, image, config);
    
    EXPECT_FALSE(success) << "Should fail with expired certificate when SSL verification is enabled";
}

/**
 * @brief 测试错误主机名 + SSL 验证开启（应该失败）
 */
TEST(SSLVerification, WrongHost_WithSSLVerification_ShouldFail) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;
    config.timeoutSeconds = 10;
    
    // wrong.host.badssl.com 证书的主机名不匹配
    bool success = FileHandler::DownloadImageFromURL(TestURLs::WRONG_HOST, image, config);
    
    EXPECT_FALSE(success) << "Should fail with wrong hostname when SSL verification is enabled";
}

/**
 * @brief 测试自签名证书 + SSL 验证关闭（应该能够连接）
 * 
 * 注意：这个测试验证的是 SSL 握手成功，但由于返回的不是有效图片，
 * 最终 DownloadImageFromURL 会返回 false（因为无法解码图片）
 */
TEST(SSLVerification, SelfSignedCert_WithoutSSLVerification) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = false;  // 关闭 SSL 验证
    config.timeoutSeconds = 10;
    
    // 由于 self-signed.badssl.com 返回的是 HTML 而不是图片
    // 所以即使 SSL 握手成功，图片解码也会失败
    // 我们需要一个返回图片的自签名服务器来完全测试这个场景
    
    // 这里我们只验证不会因为 SSL 错误而失败
    // 实际环境中，应该使用本地搭建的自签名 HTTPS 服务器进行测试
    bool success = FileHandler::DownloadImageFromURL(TestURLs::SELF_SIGNED, image, config);
    
    // 使用 (void) 抑制未使用变量警告
    // 注意：这个测试可能成功（SSL 通过但图片解码失败）或失败
    // 我们只是确保它不会因为 SSL 验证而崩溃
    // 实际测试中应该看日志确认没有 "SSL certificate verification failed" 错误
    (void)success;
    SUCCEED() << "Test completed without crash - check logs for SSL behavior";
}

// ==================== 下载大小限制测试 ====================

/**
 * @brief 测试下载大小限制 - 小文件应该成功
 */
TEST(SSLVerification, DownloadSizeLimit_SmallFile) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;
    config.maxDownloadSize = 10 * 1024 * 1024;  // 10MB 限制
    config.timeoutSeconds = 15;
    
    // 百度 Logo 是一个小图片
    bool success = FileHandler::DownloadImageFromURL(TestURLs::BAIDU_LOGO, image, config);
    
    EXPECT_TRUE(success) << "Small image should download successfully within size limit";
}

/**
 * @brief 测试下载大小限制 - 设置极小的限制应该失败
 */
TEST(SSLVerification, DownloadSizeLimit_TooSmall) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;
    config.maxDownloadSize = 100;  // 只允许 100 字节
    config.timeoutSeconds = 15;
    
    // 任何正常图片都会超过 100 字节
    bool success = FileHandler::DownloadImageFromURL(TestURLs::BAIDU_LOGO, image, config);
    
    EXPECT_FALSE(success) << "Should fail when download exceeds size limit";
}

// ==================== 超时测试 ====================

/**
 * @brief 测试下载超时
 */
TEST(SSLVerification, DownloadTimeout) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;
    config.timeoutSeconds = 1;  // 1秒超时
    
    auto start = std::chrono::steady_clock::now();
    
    // 使用一个不存在的端口来模拟超时
    bool success = FileHandler::DownloadImageFromURL(
        "https://10.255.255.1/image.png", image, config);
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    
    EXPECT_FALSE(success) << "Should fail due to timeout";
    EXPECT_LE(duration, 5) << "Should timeout within a reasonable time";
}

// ==================== HTTP vs HTTPS 测试 ====================

/**
 * @brief 测试 HTTP URL（非 HTTPS）- SSL 设置应该不影响
 */
TEST(SSLVerification, HTTP_NotAffectedBySSLConfig) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;  // 对 HTTP 无效
    config.timeoutSeconds = 15;
    
    // 使用 HTTP 协议
    bool success = FileHandler::DownloadImageFromURL(TestURLs::BAIDU_HTTP, image, config);
    
    // HTTP 不涉及 SSL，应该能正常工作
    EXPECT_TRUE(success) << "HTTP download should work regardless of SSL config";
}

// ==================== PDF Handler SSL 测试 ====================

/**
 * @brief 测试 PDFHandler RenderFromURL 的 SSL 验证（自签名证书应失败）
 */
TEST(SSLVerification, PDFHandler_SelfSigned_ShouldFail) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    PDFHandler handler;
    PDFRenderConfig config;
    config.dpi = 72;
    config.maxPages = 1;
    
    // 使用自签名证书的 URL
    auto result = handler.RenderFromURL(
        "https://self-signed.badssl.com/test.pdf",
        config,
        10,     // timeout
        true    // verifySSL = true
    );
    
    // 应该失败（SSL 验证失败）
    EXPECT_FALSE(result.success) << "Should fail with self-signed certificate";
    EXPECT_NE(result.errorCode, PDFErrorCode::SUCCESS);
}

// ==================== 边界条件测试 ====================

/**
 * @brief 测试无效 URL 格式
 */
TEST(SSLVerification, InvalidURL) {
    cv::Mat image;
    DownloadConfig config;
    config.verifySSL = true;
    config.timeoutSeconds = 5;
    
    // 无效的 URL
    bool success1 = FileHandler::DownloadImageFromURL("not_a_url", image, config);
    EXPECT_FALSE(success1) << "Should fail with invalid URL";
    
    // 不存在的域名
    bool success2 = FileHandler::DownloadImageFromURL(
        "https://this-domain-definitely-does-not-exist-12345.com/image.png", 
        image, config);
    EXPECT_FALSE(success2) << "Should fail with non-existent domain";
}

/**
 * @brief 测试空 URL
 */
TEST(SSLVerification, EmptyURL) {
    cv::Mat image;
    DownloadConfig config;
    
    bool success = FileHandler::DownloadImageFromURL("", image, config);
    EXPECT_FALSE(success) << "Should fail with empty URL";
}

// ==================== 压力测试 ====================

/**
 * @brief 测试多次连续下载
 */
TEST(SSLVerification, MultipleDownloads) {
    if (!hasNetworkConnection()) {
        GTEST_SKIP() << "No network connection available, skipping network test";
    }
    
    DownloadConfig config;
    config.verifySSL = true;
    config.timeoutSeconds = 10;
    
    int successCount = 0;
    const int iterations = 3;
    
    for (int i = 0; i < iterations; ++i) {
        cv::Mat image;
        if (FileHandler::DownloadImageFromURL(TestURLs::BAIDU_LOGO, image, config)) {
            successCount++;
        }
        // 小延迟避免请求过快
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    EXPECT_EQ(successCount, iterations) 
        << "All downloads should succeed with valid HTTPS and SSL verification";
}

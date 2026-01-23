#pragma once

/**
 * @file pdf_handler.h
 * @brief PDF 文件处理器 - 使用 Poppler 将 PDF 渲染为图像
 * 
 * 功能：
 * - 支持 Base64 和 URL 输入
 * - 并行渲染多页 PDF（受信号量限制）
 * - 完整的错误处理
 * - 内存控制（maxPages, maxDpi, maxPixelsPerPage）
 */

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

// Poppler 前向声明
namespace poppler {
    class document;
    class page;
    class page_renderer;
}

namespace ocr_server {

// ==================== C++17 兼容的计数信号量 ====================

/**
 * @brief C++17 兼容的计数信号量实现
 * 
 * 使用 std::mutex + std::condition_variable 模拟 std::counting_semaphore (C++20)
 */
class CountingSemaphore {
public:
    explicit CountingSemaphore(int initial_count) 
        : count_(initial_count) {}
    
    // 禁止拷贝
    CountingSemaphore(const CountingSemaphore&) = delete;
    CountingSemaphore& operator=(const CountingSemaphore&) = delete;
    
    /**
     * @brief 获取信号量（阻塞直到可用）
     */
    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return count_ > 0; });
        --count_;
    }
    
    /**
     * @brief 释放信号量
     */
    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++count_;
        }
        cv_.notify_one();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

// ==================== PDF 错误码定义 ====================

/**
 * @brief PDF 处理错误码
 */
namespace PDFErrorCode {
    constexpr int SUCCESS = 0;
    
    // 配置错误 (1001)
    constexpr int CONFIG_ERROR = 1001;         // 配置参数无效
    
    // 文件/格式错误 (1002-1009)
    constexpr int FILE_ERROR = 1002;           // 文件无法打开
    constexpr int FORMAT_ERROR = 1003;         // 非 PDF 或已损坏
    constexpr int PASSWORD_REQUIRED = 1004;    // 需要密码
    constexpr int SECURITY_ERROR = 1005;       // 安全策略限制
    constexpr int PAGE_ERROR = 1006;           // 页面不存在
    constexpr int PAGE_SIZE_ERROR = 1007;      // 页面尺寸异常
    constexpr int PAGE_LIMIT_EXCEEDED = 1008;  // 超出页数限制
    constexpr int DPI_LIMIT_EXCEEDED = 1009;   // 超出 DPI 限制
    
    // 运行时错误 (2001-2003)
    constexpr int UNKNOWN_ERROR = 2001;        // 未知错误
    constexpr int MEMORY_ERROR = 2002;         // 内存分配失败
    constexpr int TIMEOUT_ERROR = 2003;        // 渲染超时
}

// ==================== PDF 处理常量 ====================

/**
 * @brief PDF 处理相关的常量定义
 */
namespace PDFConstants {
    // PDF 标准常量
    constexpr double POINTS_PER_INCH = 72.0;          // PDF 点数/英寸（PDF 标准）
    
    // 默认配置值
    constexpr int DEFAULT_DPI = 150;                  // 默认渲染 DPI
    constexpr int DEFAULT_MAX_PAGES = 10;             // 默认最大页数
    constexpr int DEFAULT_MAX_DPI = 300;              // 默认 DPI 上限
    constexpr int DEFAULT_MAX_PIXELS_PER_PAGE = 25000000;  // 默认单页最大像素数 (5000x5000)
    constexpr int DEFAULT_RENDER_TIMEOUT_MS = 30000;  // 默认渲染超时 (30秒)
    constexpr int DEFAULT_MAX_CONCURRENT_RENDERS = 4; // 默认最大并发渲染数
    
    // 参数范围限制
    constexpr int MIN_DPI = 72;                       // 最小 DPI
    constexpr int MAX_DPI = 300;                      // 最大 DPI
    constexpr int MIN_PAGES = 1;                      // 最小页数
    constexpr int MAX_PAGES = 100;                    // 最大页数
    constexpr int MIN_CONCURRENT_RENDERS = 1;         // 最小并发数
    constexpr int MAX_CONCURRENT_RENDERS = 16;        // 最大并发数
    
    // 内存警告阈值
    constexpr int HIGH_MEMORY_PAGE_THRESHOLD = 10;    // 高内存使用页数阈值
    constexpr int HIGH_MEMORY_DPI_THRESHOLD = 150;    // 高内存使用 DPI 阈值
}

// ==================== 配置结构 ====================

/**
 * @brief PDF 渲染配置
 */
struct PDFRenderConfig {
    int dpi = PDFConstants::DEFAULT_DPI;
    int maxPages = PDFConstants::DEFAULT_MAX_PAGES;
    int maxDpi = PDFConstants::DEFAULT_MAX_DPI;
    int maxPixelsPerPage = PDFConstants::DEFAULT_MAX_PIXELS_PER_PAGE;
    int renderTimeoutMs = PDFConstants::DEFAULT_RENDER_TIMEOUT_MS;
    int maxConcurrentRenders = PDFConstants::DEFAULT_MAX_CONCURRENT_RENDERS;
    bool useAlpha = false;
    
    bool Validate(std::string& error_msg) const;
};

/**
 * @brief 单页渲染结果
 */
struct PDFPageImage {
    int pageIndex = -1;
    cv::Mat image;
    int originalWidthPts = 0;
    int originalHeightPts = 0;
    int renderedWidth = 0;
    int renderedHeight = 0;
    bool success = false;
    int errorCode = 0;
    std::string errorMsg;
    double renderTimeMs = 0;
};

/**
 * @brief PDF 渲染整体结果
 */
struct PDFRenderResult {
    bool success = false;
    int errorCode = 0;
    std::string errorMsg;
    int totalPages = 0;
    int renderedPages = 0;
    int failedPages = 0;
    double totalRenderTimeMs = 0;
    std::vector<PDFPageImage> pages;
};

// ==================== PDFHandler 类 ====================

/**
 * @brief PDF 处理器类（使用 Poppler 后端）
 * 
 * Poppler 提供良好的线程安全性，适合高并发场景。
 */
class PDFHandler {
public:
    PDFHandler();
    ~PDFHandler();
    
    // 禁止拷贝
    PDFHandler(const PDFHandler&) = delete;
    PDFHandler& operator=(const PDFHandler&) = delete;
    
    /**
     * @brief 从 Base64 解码并渲染 PDF
     */
    PDFRenderResult RenderFromBase64(const std::string& base64_str,
                                      const PDFRenderConfig& config = {});
    
    /**
     * @brief 从 URL 下载并渲染 PDF
     */
    PDFRenderResult RenderFromURL(const std::string& url,
                                    const PDFRenderConfig& config = {},
                                    int timeoutSeconds = 30,
                                    bool verifySSL = true);
    
    /**
     * @brief 从内存数据渲染 PDF
     */
    PDFRenderResult RenderFromMemory(const std::vector<uint8_t>& data,
                                      const PDFRenderConfig& config = {});
    
    /**
     * @brief 获取 PDF 页数（不渲染）
     */
    int GetPageCount(const std::vector<uint8_t>& data, int& errorCode);
    
    // ==================== 静态工具方法 ====================
    
    static std::string GetErrorMessage(int errorCode);
    static int GetHttpStatusCode(int errorCode);
    static std::string GetBackendName() { return "Poppler"; }
    
private:
    /**
     * @brief 并行渲染所有页面
     */
    std::vector<PDFPageImage> RenderPagesParallel(
        poppler::document* doc, int pageCount, const PDFRenderConfig& config);
    
    /**
     * @brief 渲染单页
     */
    PDFPageImage RenderSinglePage(poppler::document* doc, int pageIndex,
                                   const PDFRenderConfig& config);
    
    // 渲染并发控制
    std::unique_ptr<CountingSemaphore> render_semaphore_;
    
    // 渲染互斥锁
    mutable std::mutex render_mutex_;
};

} // namespace ocr_server

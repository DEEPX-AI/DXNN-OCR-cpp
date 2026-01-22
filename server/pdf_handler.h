#pragma once

/**
 * @file pdf_handler.h
 * @brief PDF 文件处理器 - 使用 PDFium 将 PDF 渲染为图像
 * 
 * 功能：
 * - 支持 Base64 和 URL 输入
 * - 并行渲染多页 PDF（受信号量限制）
 * - 完整的错误处理（PDFium 错误码映射）
 * - 内存控制（maxPages, maxDpi, maxPixelsPerPage）
 */

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

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
 * @brief PDF 处理错误码（对应 PDFium FPDF_ERR_* 映射）
 */
namespace PDFErrorCode {
    constexpr int SUCCESS = 0;
    
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

// ==================== 配置结构 ====================

/**
 * @brief PDF 渲染配置
 */
struct PDFRenderConfig {
    int dpi = 150;                      // 渲染 DPI (默认 150，参考 PaddleOCR)
    int maxPages = 10;                  // 最大处理页数 (默认 10，参考 PaddleOCR)
    int maxDpi = 300;                   // DPI 上限
    int maxPixelsPerPage = 25000000;    // 单页最大像素数 (5000x5000)
    int renderTimeoutMs = 30000;        // 单页渲染超时 (30s)
    int maxConcurrentRenders = 4;       // 最大并发渲染页数
    bool useAlpha = false;              // 是否保留透明通道
    
    // 验证配置
    bool Validate(std::string& error_msg) const;
};

/**
 * @brief 单页渲染结果
 */
struct PDFPageImage {
    int pageIndex = -1;             // 页码 (0-based)
    cv::Mat image;                  // 渲染后的图像
    int originalWidthPts = 0;       // 原始 PDF 页面宽度 (点, 1点=1/72英寸)
    int originalHeightPts = 0;      // 原始 PDF 页面高度 (点)
    int renderedWidth = 0;          // 渲染后图像宽度 (像素)
    int renderedHeight = 0;         // 渲染后图像高度 (像素)
    bool success = false;           // 该页是否渲染成功
    int errorCode = 0;              // 错误码 (如果失败)
    std::string errorMsg;           // 错误信息 (如果失败)
    double renderTimeMs = 0;        // 渲染耗时 (毫秒)
};

/**
 * @brief PDF 渲染整体结果
 */
struct PDFRenderResult {
    bool success = false;           // 是否全部成功
    int errorCode = 0;              // 主要错误码
    std::string errorMsg;           // 主要错误信息
    int totalPages = 0;             // PDF 总页数
    int renderedPages = 0;          // 实际渲染页数
    int failedPages = 0;            // 失败页数
    double totalRenderTimeMs = 0;   // 总渲染耗时
    std::vector<PDFPageImage> pages; // 各页结果
};

// ==================== PDFHandler 类 ====================

/**
 * @brief PDF 处理器类
 * 
 * 使用 PDFium 库将 PDF 文件渲染为 cv::Mat 图像。
 * 支持并行渲染（受信号量限制），完整的错误处理。
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
     * @param base64_str Base64 编码的 PDF 数据（可带 data:application/pdf;base64, 前缀）
     * @param config 渲染配置
     * @return PDFRenderResult 渲染结果
     */
    PDFRenderResult RenderFromBase64(const std::string& base64_str,
                                      const PDFRenderConfig& config = {});
    
    /**
     * @brief 从 URL 下载并渲染 PDF
     * @param url PDF 文件 URL
     * @param config 渲染配置
     * @param timeoutSeconds 下载超时时间
     * @param verifySSL 是否验证 SSL 证书
     * @return PDFRenderResult 渲染结果
     */
    PDFRenderResult RenderFromURL(const std::string& url,
                                    const PDFRenderConfig& config = {},
                                    int timeoutSeconds = 30,
                                    bool verifySSL = true);
    
    /**
     * @brief 从内存数据渲染 PDF
     * @param data PDF 文件原始数据
     * @param config 渲染配置
     * @return PDFRenderResult 渲染结果
     */
    PDFRenderResult RenderFromMemory(const std::vector<uint8_t>& data,
                                      const PDFRenderConfig& config = {});
    
    /**
     * @brief 获取 PDF 页数（不渲染）
     * @param data PDF 文件原始数据
     * @param errorCode 输出错误码
     * @return 页数，失败返回 -1
     */
    int GetPageCount(const std::vector<uint8_t>& data, int& errorCode);
    
    // ==================== 静态工具方法 ====================
    
    /**
     * @brief 将 PDFium 错误码转换为业务错误码
     */
    static int MapPDFiumError(unsigned long pdfiumError);
    
    /**
     * @brief 获取错误码对应的错误信息
     */
    static std::string GetErrorMessage(int errorCode);
    
    /**
     * @brief 获取建议的 HTTP 状态码
     */
    static int GetHttpStatusCode(int errorCode);
    
private:
    /**
     * @brief 初始化 PDFium 库（全局只调用一次）
     */
    static void InitializePDFium();
    
    /**
     * @brief 渲染单页（线程安全）
     */
    PDFPageImage RenderSinglePage(void* doc, int pageIndex, 
                                   const PDFRenderConfig& config);
    
    /**
     * @brief 并行渲染所有页面
     */
    std::vector<PDFPageImage> RenderPagesParallel(void* doc, int pageCount,
                                                   const PDFRenderConfig& config);
    
    // PDFium 初始化标志
    static std::once_flag init_flag_;
    static std::atomic<bool> initialized_;
    
    // 渲染并发控制
    std::unique_ptr<CountingSemaphore> render_semaphore_;
    
    // PDFium 页面加载互斥锁（FPDF_LoadPage 不是线程安全的）
    mutable std::mutex page_load_mutex_;
};

} // namespace ocr_server

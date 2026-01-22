#include "pdf_handler.h"
#include "file_handler.h"
#include "common/logger.hpp"
#include "base64.h"

#include <fpdfview.h>
#include <future>
#include <chrono>
#include <curl/curl.h>

namespace ocr_server {

// ==================== 静态成员初始化 ====================

std::once_flag PDFHandler::init_flag_;
std::atomic<bool> PDFHandler::initialized_{false};

// ==================== PDFRenderConfig ====================

bool PDFRenderConfig::Validate(std::string& error_msg) const {
    if (dpi < 72 || dpi > maxDpi) {
        error_msg = fmt::format("pdfDpi must be in range [72, {}]", maxDpi);
        return false;
    }
    
    if (maxPages < 1 || maxPages > 100) {
        error_msg = "pdfMaxPages must be in range [1, 100]";
        return false;
    }
    
    if (maxConcurrentRenders < 1 || maxConcurrentRenders > 16) {
        error_msg = "maxConcurrentRenders must be in range [1, 16]";
        return false;
    }
    
    return true;
}

// ==================== PDFHandler 构造/析构 ====================

PDFHandler::PDFHandler() {
    InitializePDFium();
    
    // 初始化渲染信号量（默认最多 4 个并发渲染）
    render_semaphore_ = std::make_unique<CountingSemaphore>(4);
    
    LOG_INFO("PDFHandler created");
}

PDFHandler::~PDFHandler() {
    // PDFium 库是全局的，不在这里销毁
    LOG_INFO("PDFHandler destroyed");
}

void PDFHandler::InitializePDFium() {
    std::call_once(init_flag_, []() {
        LOG_INFO("Initializing PDFium library...");
        FPDF_InitLibrary();
        initialized_.store(true);
        LOG_INFO("PDFium library initialized");
    });
}

// ==================== 错误处理 ====================

int PDFHandler::MapPDFiumError(unsigned long pdfiumError) {
    switch (pdfiumError) {
        case FPDF_ERR_SUCCESS:  return PDFErrorCode::SUCCESS;
        case FPDF_ERR_FILE:     return PDFErrorCode::FILE_ERROR;
        case FPDF_ERR_FORMAT:   return PDFErrorCode::FORMAT_ERROR;
        case FPDF_ERR_PASSWORD: return PDFErrorCode::PASSWORD_REQUIRED;
        case FPDF_ERR_SECURITY: return PDFErrorCode::SECURITY_ERROR;
        case FPDF_ERR_PAGE:     return PDFErrorCode::PAGE_ERROR;
        default:                return PDFErrorCode::UNKNOWN_ERROR;
    }
}

std::string PDFHandler::GetErrorMessage(int errorCode) {
    switch (errorCode) {
        case PDFErrorCode::SUCCESS:
            return "Success";
        case PDFErrorCode::FILE_ERROR:
            return "PDF file cannot be opened";
        case PDFErrorCode::FORMAT_ERROR:
            return "Invalid PDF format or corrupted file";
        case PDFErrorCode::PASSWORD_REQUIRED:
            return "PDF is password protected";
        case PDFErrorCode::SECURITY_ERROR:
            return "PDF security policy not supported";
        case PDFErrorCode::PAGE_ERROR:
            return "PDF page not found";
        case PDFErrorCode::PAGE_SIZE_ERROR:
            return "PDF page size exceeds maximum limit";
        case PDFErrorCode::PAGE_LIMIT_EXCEEDED:
            return "PDF page count exceeds maximum limit";
        case PDFErrorCode::DPI_LIMIT_EXCEEDED:
            return "Requested DPI exceeds maximum limit";
        case PDFErrorCode::MEMORY_ERROR:
            return "Memory allocation failed during PDF rendering";
        case PDFErrorCode::TIMEOUT_ERROR:
            return "PDF page rendering timeout";
        case PDFErrorCode::UNKNOWN_ERROR:
        default:
            return "Unknown PDF processing error";
    }
}

int PDFHandler::GetHttpStatusCode(int errorCode) {
    switch (errorCode) {
        case PDFErrorCode::SUCCESS:
            return 200;
        case PDFErrorCode::PASSWORD_REQUIRED:
            return 401;
        case PDFErrorCode::SECURITY_ERROR:
            return 403;
        case PDFErrorCode::MEMORY_ERROR:
            return 503;
        case PDFErrorCode::TIMEOUT_ERROR:
            return 504;
        case PDFErrorCode::UNKNOWN_ERROR:
            return 500;
        default:
            return 400;  // 大多数是客户端错误
    }
}

// ==================== 渲染实现 ====================

PDFRenderResult PDFHandler::RenderFromBase64(const std::string& base64_str,
                                              const PDFRenderConfig& config) {
    PDFRenderResult result;
    
    // 移除可能的 "data:application/pdf;base64," 前缀
    std::string clean_base64 = base64_str;
    size_t comma_pos = base64_str.find(',');
    if (comma_pos != std::string::npos) {
        clean_base64 = base64_str.substr(comma_pos + 1);
    }
    
    // Base64 解码
    std::string decoded;
    try {
        decoded = base64_decode(clean_base64, false);
    } catch (const std::exception& e) {
        result.errorCode = PDFErrorCode::FORMAT_ERROR;
        result.errorMsg = std::string("Base64 decode failed: ") + e.what();
        LOG_ERROR("PDF Base64 decode failed: {}", e.what());
        return result;
    }
    
    if (decoded.empty()) {
        result.errorCode = PDFErrorCode::FORMAT_ERROR;
        result.errorMsg = "Base64 decode resulted in empty data";
        LOG_ERROR("PDF Base64 decode resulted in empty data");
        return result;
    }
    
    // 转换为 vector 并渲染
    std::vector<uint8_t> data(decoded.begin(), decoded.end());
    return RenderFromMemory(data, config);
}

PDFRenderResult PDFHandler::RenderFromURL(const std::string& url,
                                            const PDFRenderConfig& config,
                                            int timeoutSeconds,
                                            bool verifySSL) {
    PDFRenderResult result;
    
    LOG_INFO("Downloading PDF from URL: {}", url.substr(0, 100));
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.errorCode = PDFErrorCode::UNKNOWN_ERROR;
        result.errorMsg = "Failed to initialize CURL";
        LOG_ERROR("Failed to initialize CURL for PDF download");
        return result;
    }
    
    std::vector<uint8_t> buffer;
    
    // CURL 回调
    auto writeCallback = [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
        size_t total_size = size * nmemb;
        std::vector<uint8_t>* buf = static_cast<std::vector<uint8_t>*>(userp);
        buf->insert(buf->end(), static_cast<uint8_t*>(contents),
                    static_cast<uint8_t*>(contents) + total_size);
        return total_size;
    };
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
                     static_cast<size_t(*)(void*, size_t, size_t, void*)>(writeCallback));
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (verifySSL) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        LOG_WARN("SSL verification disabled for PDF download - NOT recommended for production!");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        result.errorCode = PDFErrorCode::FILE_ERROR;
        result.errorMsg = std::string("Download failed: ") + curl_easy_strerror(res);
        LOG_ERROR("PDF download failed: {}", curl_easy_strerror(res));
        return result;
    }
    
    if (http_code != 200) {
        result.errorCode = PDFErrorCode::FILE_ERROR;
        result.errorMsg = fmt::format("HTTP error: {}", http_code);
        LOG_ERROR("PDF download HTTP error: {}", http_code);
        return result;
    }
    
    if (buffer.empty()) {
        result.errorCode = PDFErrorCode::FILE_ERROR;
        result.errorMsg = "Downloaded empty PDF file";
        LOG_ERROR("Downloaded empty PDF file from URL");
        return result;
    }
    
    LOG_INFO("Downloaded PDF: {} bytes", buffer.size());
    return RenderFromMemory(buffer, config);
}

PDFRenderResult PDFHandler::RenderFromMemory(const std::vector<uint8_t>& data,
                                              const PDFRenderConfig& config) {
    PDFRenderResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 验证配置
    std::string configError;
    if (!config.Validate(configError)) {
        result.errorCode = PDFErrorCode::DPI_LIMIT_EXCEEDED;
        result.errorMsg = configError;
        LOG_ERROR("Invalid PDF config: {}", configError);
        return result;
    }
    
    // 2. 加载 PDF 文档
    FPDF_DOCUMENT doc = FPDF_LoadMemDocument(data.data(), 
                                              static_cast<int>(data.size()), 
                                              nullptr);
    if (!doc) {
        unsigned long pdfiumError = FPDF_GetLastError();
        result.errorCode = MapPDFiumError(pdfiumError);
        result.errorMsg = GetErrorMessage(result.errorCode);
        LOG_ERROR("Failed to load PDF: {} (PDFium error: {})", 
                  result.errorMsg, pdfiumError);
        return result;
    }
    
    // 3. 获取页数
    result.totalPages = FPDF_GetPageCount(doc);
    LOG_INFO("PDF loaded: {} total pages", result.totalPages);
    
    if (result.totalPages == 0) {
        FPDF_CloseDocument(doc);
        result.errorCode = PDFErrorCode::FORMAT_ERROR;
        result.errorMsg = "PDF has no pages";
        LOG_ERROR("PDF has no pages");
        return result;
    }
    
    // 4. 确定要渲染的页数
    int pagesToRender = std::min(result.totalPages, config.maxPages);
    
    if (result.totalPages > config.maxPages) {
        LOG_WARN("PDF has {} pages, limiting to {} (maxPages={})", 
                 result.totalPages, pagesToRender, config.maxPages);
    }
    
    // 5. 并行渲染所有页面
    result.pages = RenderPagesParallel(doc, pagesToRender, config);
    result.renderedPages = static_cast<int>(result.pages.size());
    
    // 6. 关闭文档
    FPDF_CloseDocument(doc);
    
    // 7. 统计结果
    result.failedPages = 0;
    for (const auto& page : result.pages) {
        if (!page.success) {
            result.failedPages++;
        }
        result.totalRenderTimeMs += page.renderTimeMs;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.totalRenderTimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();
    
    // 8. 设置整体状态
    if (result.failedPages == 0) {
        result.success = true;
        result.errorCode = PDFErrorCode::SUCCESS;
        result.errorMsg = "Success";
    } else if (result.failedPages < result.renderedPages) {
        // 部分成功
        result.success = true;  // 允许部分失败
        result.errorCode = PDFErrorCode::SUCCESS;
        result.errorMsg = fmt::format("{} of {} pages failed to render", 
                                       result.failedPages, result.renderedPages);
        LOG_WARN("{}", result.errorMsg);
    } else {
        // 全部失败
        result.success = false;
        result.errorCode = result.pages.empty() ? 
            PDFErrorCode::UNKNOWN_ERROR : result.pages[0].errorCode;
        result.errorMsg = result.pages.empty() ? 
            "All pages failed to render" : result.pages[0].errorMsg;
    }
    
    LOG_INFO("PDF rendering completed: {} pages in {:.2f}ms ({} failed)", 
             result.renderedPages, result.totalRenderTimeMs, result.failedPages);
    
    return result;
}

std::vector<PDFPageImage> PDFHandler::RenderPagesParallel(void* doc, int pageCount,
                                                          const PDFRenderConfig& config) {
    std::vector<std::future<PDFPageImage>> futures;
    futures.reserve(pageCount);
    
    LOG_INFO("Starting parallel render of {} pages (max concurrent: {})", 
             pageCount, config.maxConcurrentRenders);
    
    // 复制 config 避免引用捕获问题
    PDFRenderConfig configCopy = config;
    
    // 并行提交渲染任务
    for (int i = 0; i < pageCount; ++i) {
        futures.push_back(std::async(std::launch::async, 
            [this, doc, i, configCopy]() -> PDFPageImage {
                // 获取渲染许可（限制并发数）
                render_semaphore_->acquire();
                
                PDFPageImage result;
                try {
                    result = RenderSinglePage(doc, i, configCopy);
                } catch (const std::exception& e) {
                    result.pageIndex = i;
                    result.success = false;
                    result.errorCode = PDFErrorCode::UNKNOWN_ERROR;
                    result.errorMsg = std::string("Exception: ") + e.what();
                    LOG_ERROR("Exception rendering page {}: {}", i, e.what());
                }
                
                // 释放许可
                render_semaphore_->release();
                
                return result;
            }));
    }
    
    // 收集结果（保持页码顺序）
    std::vector<PDFPageImage> pages;
    pages.reserve(pageCount);
    
    for (int i = 0; i < pageCount; ++i) {
        pages.push_back(futures[i].get());
    }
    
    return pages;
}

PDFPageImage PDFHandler::RenderSinglePage(void* doc, int pageIndex,
                                           const PDFRenderConfig& config) {
    PDFPageImage result;
    result.pageIndex = pageIndex;
    result.success = false;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // PDFium 渲染操作不是线程安全的，整个过程需要互斥锁保护
    std::lock_guard<std::mutex> lock(page_load_mutex_);
    
    // 1. 加载页面
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(doc), pageIndex);
    if (!page) {
        result.errorCode = PDFErrorCode::PAGE_ERROR;
        result.errorMsg = fmt::format("Failed to load page {}", pageIndex);
        LOG_ERROR("Failed to load PDF page {}", pageIndex);
        return result;
    }
    
    // 2. 获取页面尺寸
    double widthPts = FPDF_GetPageWidth(page);
    double heightPts = FPDF_GetPageHeight(page);
    result.originalWidthPts = static_cast<int>(widthPts);
    result.originalHeightPts = static_cast<int>(heightPts);
    
    // 3. 计算渲染尺寸
    double scale = config.dpi / 72.0;
    int renderWidth = static_cast<int>(widthPts * scale);
    int renderHeight = static_cast<int>(heightPts * scale);
    result.renderedWidth = renderWidth;
    result.renderedHeight = renderHeight;
    
    // 4. 检查像素限制
    int64_t totalPixels = static_cast<int64_t>(renderWidth) * renderHeight;
    if (totalPixels > config.maxPixelsPerPage) {
        FPDF_ClosePage(page);
        result.errorCode = PDFErrorCode::PAGE_SIZE_ERROR;
        result.errorMsg = fmt::format("Page {} size {}x{} ({} pixels) exceeds limit {}", 
                                       pageIndex, renderWidth, renderHeight,
                                       totalPixels, config.maxPixelsPerPage);
        LOG_WARN("{}", result.errorMsg);
        return result;
    }
    
    // 5. 创建位图
    FPDF_BITMAP bitmap = FPDFBitmap_Create(renderWidth, renderHeight, 
                                            config.useAlpha ? 1 : 0);
    if (!bitmap) {
        FPDF_ClosePage(page);
        result.errorCode = PDFErrorCode::MEMORY_ERROR;
        result.errorMsg = fmt::format("Failed to allocate bitmap for page {} ({}x{})", 
                                       pageIndex, renderWidth, renderHeight);
        LOG_ERROR("{}", result.errorMsg);
        return result;
    }
    
    // 6. 填充白色背景
    FPDFBitmap_FillRect(bitmap, 0, 0, renderWidth, renderHeight, 0xFFFFFFFF);
    
    // 7. 渲染页面到位图
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, renderWidth, renderHeight, 0, 0);
    
    // 8. 获取位图数据并转换为 cv::Mat
    void* buffer = FPDFBitmap_GetBuffer(bitmap);
    int stride = FPDFBitmap_GetStride(bitmap);
    
    // PDFium 使用 BGRA 格式 - 必须在释放 bitmap 前完成拷贝
    cv::Mat bgraImage(renderHeight, renderWidth, CV_8UC4, buffer, stride);
    
    // 转换为 BGR（OpenCV 标准格式）- 这会拷贝数据
    if (config.useAlpha) {
        result.image = bgraImage.clone();
    } else {
        cv::cvtColor(bgraImage, result.image, cv::COLOR_BGRA2BGR);
    }
    
    // 9. 清理 PDFium 资源
    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.renderTimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();
    
    result.success = true;
    result.errorCode = PDFErrorCode::SUCCESS;
    
    LOG_DEBUG("Rendered page {}: {}x{} in {:.2f}ms", 
              pageIndex, renderWidth, renderHeight, result.renderTimeMs);
    
    return result;
}

int PDFHandler::GetPageCount(const std::vector<uint8_t>& data, int& errorCode) {
    FPDF_DOCUMENT doc = FPDF_LoadMemDocument(data.data(), 
                                              static_cast<int>(data.size()), 
                                              nullptr);
    if (!doc) {
        errorCode = MapPDFiumError(FPDF_GetLastError());
        return -1;
    }
    
    int pageCount = FPDF_GetPageCount(doc);
    FPDF_CloseDocument(doc);
    
    errorCode = PDFErrorCode::SUCCESS;
    return pageCount;
}

} // namespace ocr_server

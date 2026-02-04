#include "pdf_handler.h"
#include "file_handler.h"
#include "common/logger.hpp"
#include "base64.h"

#include <future>
#include <chrono>
#include <curl/curl.h>

// Poppler C++ API
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <poppler/cpp/poppler-image.h>

namespace ocr_server {

// ==================== PDFRenderConfig ====================

bool PDFRenderConfig::Validate(std::string& error_msg) const {
    if (dpi < PDFConstants::MIN_DPI || dpi > maxDpi) {
        error_msg = fmt::format("pdfDpi must be in range [{}, {}]", 
                                PDFConstants::MIN_DPI, maxDpi);
        return false;
    }
    
    if (maxPages < PDFConstants::MIN_PAGES || maxPages > PDFConstants::MAX_PAGES) {
        error_msg = fmt::format("pdfMaxPages must be in range [{}, {}]",
                                PDFConstants::MIN_PAGES, PDFConstants::MAX_PAGES);
        return false;
    }
    
    if (maxConcurrentRenders < PDFConstants::MIN_CONCURRENT_RENDERS || 
        maxConcurrentRenders > PDFConstants::MAX_CONCURRENT_RENDERS) {
        error_msg = fmt::format("maxConcurrentRenders must be in range [{}, {}]",
                                PDFConstants::MIN_CONCURRENT_RENDERS, 
                                PDFConstants::MAX_CONCURRENT_RENDERS);
        return false;
    }
    
    return true;
}

// ==================== PDFHandler 构造/析构 ====================

PDFHandler::PDFHandler() {
    render_semaphore_ = std::make_unique<CountingSemaphore>(
        PDFConstants::DEFAULT_MAX_CONCURRENT_RENDERS);
    
    LOG_INFO("PDFHandler created with Poppler backend, max {} concurrent renders", 
             PDFConstants::DEFAULT_MAX_CONCURRENT_RENDERS);
}

PDFHandler::~PDFHandler() {
    LOG_INFO("PDFHandler destroyed");
}

// ==================== 错误处理 ====================

std::string PDFHandler::GetErrorMessage(int errorCode) {
    switch (errorCode) {
        case PDFErrorCode::SUCCESS:
            return "Success";
        case PDFErrorCode::CONFIG_ERROR:
            return "Invalid PDF configuration parameters";
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
        case PDFErrorCode::CONFIG_ERROR:
            return 400;
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
            return 400;
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
        result.errorCode = PDFErrorCode::CONFIG_ERROR;
        result.errorMsg = configError;
        LOG_ERROR("Invalid PDF config: {}", configError);
        return result;
    }
    
    // 2. 加载 PDF 文档
    poppler::document* doc = poppler::document::load_from_raw_data(
        reinterpret_cast<const char*>(data.data()), 
        static_cast<int>(data.size()));
    
    if (!doc) {
        result.errorCode = PDFErrorCode::FORMAT_ERROR;
        result.errorMsg = "Failed to load PDF document";
        LOG_ERROR("Failed to load PDF with Poppler");
        return result;
    }
    
    // 检查是否加密
    if (doc->is_locked()) {
        delete doc;
        result.errorCode = PDFErrorCode::PASSWORD_REQUIRED;
        result.errorMsg = "PDF is password protected";
        LOG_ERROR("PDF is password protected");
        return result;
    }
    
    // 3. 获取页数
    result.totalPages = doc->pages();
    LOG_INFO("PDF loaded: {} total pages", result.totalPages);
    
    if (result.totalPages == 0) {
        delete doc;
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
    delete doc;
    
    // 7. 统计结果
    result.failedPages = 0;
    for (const auto& page : result.pages) {
        if (!page.success) {
            result.failedPages++;
        }
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
        result.success = true;
        result.errorCode = PDFErrorCode::SUCCESS;
        result.errorMsg = fmt::format("{} of {} pages failed to render", 
                                       result.failedPages, result.renderedPages);
        LOG_WARN("{}", result.errorMsg);
    } else {
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

std::vector<PDFPageImage> PDFHandler::RenderPagesParallel(
    poppler::document* doc, int pageCount, const PDFRenderConfig& config) {
    
    LOG_INFO("Starting PDF rendering: {} pages (max concurrent: {})", 
             pageCount, config.maxConcurrentRenders);
    
    // ========== 阶段 1: 预加载所有页面 ==========
    LOG_DEBUG("Phase 1: Preloading {} pages...", pageCount);
    
    struct PreloadedPage {
        std::unique_ptr<poppler::page> page;
        poppler::rectf rect;
        bool valid = false;
    };
    
    std::vector<PreloadedPage> preloadedPages(pageCount);
    
    {
        std::lock_guard<std::mutex> lock(render_mutex_);
        for (int i = 0; i < pageCount; ++i) {
            preloadedPages[i].page.reset(doc->create_page(i));
            if (preloadedPages[i].page) {
                preloadedPages[i].rect = preloadedPages[i].page->page_rect();
                preloadedPages[i].valid = true;
            } else {
                LOG_ERROR("Failed to create page {}", i);
                preloadedPages[i].valid = false;
            }
        }
    }
    
    LOG_DEBUG("Phase 1 complete: All pages preloaded");
    
    // ========== 阶段 2: 并行渲染 ==========
    LOG_DEBUG("Phase 2: Rendering {} pages in parallel...", pageCount);
    
    std::vector<std::future<PDFPageImage>> futures;
    futures.reserve(pageCount);
    
    PDFRenderConfig configCopy = config;
    
    for (int i = 0; i < pageCount; ++i) {
        futures.push_back(std::async(std::launch::async, 
            [this, &preloadedPages, i, configCopy]() -> PDFPageImage {
                render_semaphore_->acquire();
                
                PDFPageImage result;
                result.pageIndex = i;
                result.success = false;
                
                try {
                    const auto& pageData = preloadedPages[i];
                    
                    if (!pageData.valid || !pageData.page) {
                        result.errorCode = PDFErrorCode::PAGE_ERROR;
                        result.errorMsg = fmt::format("Page {} was not loaded", i);
                        render_semaphore_->release();
                        return result;
                    }
                    
                    auto startTime = std::chrono::high_resolution_clock::now();
                    
                    double widthPts = pageData.rect.width();
                    double heightPts = pageData.rect.height();
                    
                    result.originalWidthPts = static_cast<int>(widthPts);
                    result.originalHeightPts = static_cast<int>(heightPts);
                    
                    double scale = configCopy.dpi / PDFConstants::POINTS_PER_INCH;
                    int renderWidth = static_cast<int>(widthPts * scale);
                    int renderHeight = static_cast<int>(heightPts * scale);
                    result.renderedWidth = renderWidth;
                    result.renderedHeight = renderHeight;
                    
                    int64_t totalPixels = static_cast<int64_t>(renderWidth) * renderHeight;
                    if (totalPixels > configCopy.maxPixelsPerPage) {
                        result.errorCode = PDFErrorCode::PAGE_SIZE_ERROR;
                        result.errorMsg = fmt::format(
                            "Page {} size {}x{} ({} pixels) exceeds limit {}", 
                            i, renderWidth, renderHeight, totalPixels, 
                            configCopy.maxPixelsPerPage);
                        LOG_WARN("{}", result.errorMsg);
                        render_semaphore_->release();
                        return result;
                    }
                    
                    // 每个线程使用独立的 page_renderer（线程安全）
                    poppler::page_renderer renderer;
                    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
                    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
                    
                    poppler::image img = renderer.render_page(
                        pageData.page.get(), 
                        configCopy.dpi, configCopy.dpi);
                    
                    if (!img.is_valid()) {
                        result.errorCode = PDFErrorCode::MEMORY_ERROR;
                        result.errorMsg = fmt::format("Failed to render page {}", i);
                        LOG_ERROR("{}", result.errorMsg);
                        render_semaphore_->release();
                        return result;
                    }
                    
                    // 转换 Poppler image 到 cv::Mat
                    int imgWidth = img.width();
                    int imgHeight = img.height();
                    int bytesPerRow = img.bytes_per_row();
                    const char* imgData = img.const_data();
                    
                    // 在 little-endian 系统上，ARGB32 实际存储为 BGRA 字节顺序
                    poppler::image::format_enum fmt = img.format();
                    
                    if (fmt == poppler::image::format_argb32) {
                        cv::Mat bgraImage(imgHeight, imgWidth, CV_8UC4, 
                                         const_cast<char*>(imgData), bytesPerRow);
                        if (configCopy.useAlpha) {
                            result.image = bgraImage.clone();
                        } else {
                            cv::cvtColor(bgraImage, result.image, cv::COLOR_BGRA2BGR);
                        }
                    } else if (fmt == poppler::image::format_rgb24) {
                        cv::Mat rgbImage(imgHeight, imgWidth, CV_8UC3, 
                                        const_cast<char*>(imgData), bytesPerRow);
                        cv::cvtColor(rgbImage, result.image, cv::COLOR_RGB2BGR);
                    } else {
                        LOG_WARN("Unsupported Poppler image format: {}, treating as BGRA", 
                                 static_cast<int>(fmt));
                        cv::Mat rawImage(imgHeight, imgWidth, CV_8UC4, 
                                        const_cast<char*>(imgData), bytesPerRow);
                        if (configCopy.useAlpha) {
                            result.image = rawImage.clone();
                        } else {
                            cv::cvtColor(rawImage, result.image, cv::COLOR_BGRA2BGR);
                        }
                    }
                    
                    auto endTime = std::chrono::high_resolution_clock::now();
                    result.renderTimeMs = std::chrono::duration<double, std::milli>(
                        endTime - startTime).count();
                    
                    result.success = true;
                    result.errorCode = PDFErrorCode::SUCCESS;
                    
                    LOG_DEBUG("Rendered page {}: {}x{} in {:.2f}ms", 
                              i, imgWidth, imgHeight, result.renderTimeMs);
                    
                } catch (const std::exception& e) {
                    result.errorCode = PDFErrorCode::UNKNOWN_ERROR;
                    result.errorMsg = std::string("Exception: ") + e.what();
                    LOG_ERROR("Exception rendering page {}: {}", i, e.what());
                }
                
                render_semaphore_->release();
                return result;
            }));
    }
    
    // 收集结果
    std::vector<PDFPageImage> results;
    results.reserve(pageCount);
    for (int i = 0; i < pageCount; ++i) {
        results.push_back(futures[i].get());
    }
    
    LOG_DEBUG("Phase 2 complete: All pages rendered");
    
    return results;
}

PDFPageImage PDFHandler::RenderSinglePage(poppler::document* doc, int pageIndex,
                                           const PDFRenderConfig& config) {
    PDFPageImage result;
    result.pageIndex = pageIndex;
    result.success = false;
    
    std::unique_ptr<poppler::page> page(doc->create_page(pageIndex));
    if (!page) {
        result.errorCode = PDFErrorCode::PAGE_ERROR;
        result.errorMsg = fmt::format("Failed to load page {}", pageIndex);
        LOG_ERROR("Failed to load PDF page {}", pageIndex);
        return result;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    poppler::rectf rect = page->page_rect();
    double widthPts = rect.width();
    double heightPts = rect.height();
    
    result.originalWidthPts = static_cast<int>(widthPts);
    result.originalHeightPts = static_cast<int>(heightPts);
    
    double scale = config.dpi / PDFConstants::POINTS_PER_INCH;
    int renderWidth = static_cast<int>(widthPts * scale);
    int renderHeight = static_cast<int>(heightPts * scale);
    result.renderedWidth = renderWidth;
    result.renderedHeight = renderHeight;
    
    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
    
    poppler::image img = renderer.render_page(page.get(), config.dpi, config.dpi);
    
    if (!img.is_valid()) {
        result.errorCode = PDFErrorCode::MEMORY_ERROR;
        result.errorMsg = fmt::format("Failed to render page {}", pageIndex);
        return result;
    }
    
    int imgWidth = img.width();
    int imgHeight = img.height();
    int bytesPerRow = img.bytes_per_row();
    const char* imgData = img.const_data();
    
    poppler::image::format_enum fmt = img.format();
    if (fmt == poppler::image::format_argb32) {
        cv::Mat bgraImage(imgHeight, imgWidth, CV_8UC4, 
                         const_cast<char*>(imgData), bytesPerRow);
        if (config.useAlpha) {
            result.image = bgraImage.clone();
        } else {
            cv::cvtColor(bgraImage, result.image, cv::COLOR_BGRA2BGR);
        }
    } else if (fmt == poppler::image::format_rgb24) {
        cv::Mat rgbImage(imgHeight, imgWidth, CV_8UC3, 
                        const_cast<char*>(imgData), bytesPerRow);
        cv::cvtColor(rgbImage, result.image, cv::COLOR_RGB2BGR);
    } else {
        LOG_WARN("Unsupported Poppler image format: {}", static_cast<int>(fmt));
        cv::Mat rawImage(imgHeight, imgWidth, CV_8UC4, 
                        const_cast<char*>(imgData), bytesPerRow);
        if (config.useAlpha) {
            result.image = rawImage.clone();
        } else {
            cv::cvtColor(rawImage, result.image, cv::COLOR_BGRA2BGR);
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.renderTimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();
    
    result.success = true;
    result.errorCode = PDFErrorCode::SUCCESS;
    
    return result;
}

int PDFHandler::GetPageCount(const std::vector<uint8_t>& data, int& errorCode) {
    poppler::document* doc = poppler::document::load_from_raw_data(
        reinterpret_cast<const char*>(data.data()), 
        static_cast<int>(data.size()));
    
    if (!doc) {
        errorCode = PDFErrorCode::FORMAT_ERROR;
        return -1;
    }
    
    if (doc->is_locked()) {
        delete doc;
        errorCode = PDFErrorCode::PASSWORD_REQUIRED;
        return -1;
    }
    
    int pageCount = doc->pages();
    delete doc;
    
    errorCode = PDFErrorCode::SUCCESS;
    return pageCount;
}

} // namespace ocr_server

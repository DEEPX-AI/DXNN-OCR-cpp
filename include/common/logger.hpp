#pragma once

/**
 * @file logger.hpp
 * @brief Async logging system for DeepX OCR using spdlog
 * 
 * Features:
 * - Async mode: Non-blocking log writes
 * - Rotating file sink: Auto-rotate logs with size limit
 * - Console + file output: Colored console + persistent file logs
 * 
 * Uses fmt-style formatting: LOG_INFO("value: {}", value);
 */

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <mutex>
#include <string>

namespace DeepXOCR {

/**
 * @brief Logger configuration
 */
struct LoggerConfig {
    // Async thread pool settings
    size_t asyncQueueSize = 8192;           // Async queue size (power of 2)
    size_t asyncThreadCount = 1;            // Number of async logging threads
    
    // Rotating file settings
    bool enableFileLog = true;              // Enable file logging
    std::string logDir = "logs";            // Log directory
    std::string logFileName = "deepx_ocr";  // Base log file name
    size_t maxFileSize = 10 * 1024 * 1024;  // Max file size: 10 MB
    size_t maxFiles = 5;                    // Max number of rotated files
    
    // Log pattern: [time][level][thread][file:line] message
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e][%^%l%$][%t][%s:%#] %v";
};

/**
 * @brief Get or create the global async logger instance
 */
inline std::shared_ptr<spdlog::logger>& GetLogger() {
    static std::shared_ptr<spdlog::logger> logger;
    static std::once_flag flag;
    
    std::call_once(flag, []() {
        LoggerConfig config;
        
        // Initialize async thread pool (call once before creating async loggers)
        spdlog::init_thread_pool(config.asyncQueueSize, config.asyncThreadCount);
        
        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;
        
        // Console sink (stdout with colors)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(config.pattern);
        sinks.push_back(console_sink);
        
        // Rotating file sink (if enabled)
        if (config.enableFileLog) {
            try {
                // Create log directory if needed
                std::string logPath = config.logDir + "/" + config.logFileName + ".log";
                
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    logPath,
                    config.maxFileSize,    // Max file size before rotation
                    config.maxFiles        // Max number of rotated files
                );
                file_sink->set_pattern(config.pattern);
                sinks.push_back(file_sink);
            } catch (const spdlog::spdlog_ex& ex) {
                // Fall back to console-only if file creation fails
                fprintf(stderr, "Warning: Failed to create log file: %s\n", ex.what());
            }
        }
        
        // Create async logger with multiple sinks
        logger = std::make_shared<spdlog::async_logger>(
            "DeepXOCR",
            sinks.begin(),
            sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block  // Block when queue is full (safe)
        );
        
        // Set log level based on build type
#ifndef NDEBUG
        logger->set_level(spdlog::level::debug);
#else
        logger->set_level(spdlog::level::info);
#endif
        
        // Register as default logger
        spdlog::register_logger(logger);
        
        // Flush on error or higher
        logger->flush_on(spdlog::level::err);
    });
    
    return logger;
}

/**
 * @brief Initialize logger with custom configuration
 * @param config Logger configuration
 * @note Must be called before any LOG_* macros if custom config is needed
 */
inline void InitLogger(const LoggerConfig& config) {
    // Shutdown existing logger first
    spdlog::shutdown();
    
    // Initialize async thread pool
    spdlog::init_thread_pool(config.asyncQueueSize, config.asyncThreadCount);
    
    // Create sinks
    std::vector<spdlog::sink_ptr> sinks;
    
    // Console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern(config.pattern);
    sinks.push_back(console_sink);
    
    // Rotating file sink
    if (config.enableFileLog) {
        std::string logPath = config.logDir + "/" + config.logFileName + ".log";
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logPath,
            config.maxFileSize,
            config.maxFiles
        );
        file_sink->set_pattern(config.pattern);
        sinks.push_back(file_sink);
    }
    
    // Create and register async logger
    auto& logger = GetLogger();
    logger = std::make_shared<spdlog::async_logger>(
        "DeepXOCR",
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    
    spdlog::register_logger(logger);
}

/**
 * @brief Set log level at runtime
 */
inline void SetLogLevel(spdlog::level::level_enum level) {
    GetLogger()->set_level(level);
}

/**
 * @brief Flush all pending log messages
 * @note Call this before program exit to ensure all logs are written
 */
inline void FlushLogs() {
    GetLogger()->flush();
}

/**
 * @brief Shutdown logger and release resources
 * @note Call this at program exit
 */
inline void ShutdownLogger() {
    spdlog::shutdown();
}

} // namespace DeepXOCR

// ============================================================
// Log level definitions for compile-time control
// ============================================================
#define LOG_LEVEL_OFF 1000
#define LOG_LEVEL_ERROR 500
#define LOG_LEVEL_WARN 400
#define LOG_LEVEL_INFO 300
#define LOG_LEVEL_DEBUG 200
#define LOG_LEVEL_TRACE 100
#define LOG_LEVEL_ALL 0

// Default log level
#ifndef LOG_LEVEL
#ifndef NDEBUG
#define LOG_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif
#endif

// ============================================================
// Logging Macros using spdlog with fmt-style formatting
// Usage: LOG_INFO("Hello {}", name);
//        LOG_DEBUG("Value: {:.2f}", value);
// ============================================================

// ERROR Level
#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(DeepXOCR::GetLogger(), __VA_ARGS__)
#else
#define LOG_ERROR(...) ((void)0)
#endif

// WARN Level
#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(DeepXOCR::GetLogger(), __VA_ARGS__)
#else
#define LOG_WARN(...) ((void)0)
#endif

// INFO Level
#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(DeepXOCR::GetLogger(), __VA_ARGS__)
#else
#define LOG_INFO(...) ((void)0)
#endif

// DEBUG Level
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(DeepXOCR::GetLogger(), __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

// TRACE Level
#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(DeepXOCR::GetLogger(), __VA_ARGS__)
#else
#define LOG_TRACE(...) ((void)0)
#endif

// ============================================================
// Helper Macros
// ============================================================

// Modern C++17 helper for debug-only code execution
// Usage: LOG_DEBUG_EXEC([&]{ /* debug code here */ });
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG_EXEC(lambda) lambda()
#else
#define LOG_DEBUG_EXEC(lambda) ((void)0)
#endif

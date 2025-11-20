#pragma once

#include <ctime>
#include <string>

/**
 * Logging system for DeepX OCR
 */

// Compile-time filename extraction
using cstr = const char*;

static constexpr auto PastLastSlash(cstr a, cstr b) -> cstr {
    return *a == '\0' ? b : *a == '/' ? PastLastSlash(a + 1, a + 1) : PastLastSlash(a + 1, b);
}

static constexpr auto PastLastSlash(cstr a) -> cstr {
    return PastLastSlash(a, a);
}

// Cross-platform compatible version (without GCC statement expressions)
#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: Use statement expression for efficiency
    #define __SHORT_FILE__ ({ constexpr cstr sf__{PastLastSlash(__FILE__)}; sf__; })
#else
    // MSVC and others: Use direct call (may be less efficient but compatible)
    #define __SHORT_FILE__ (PastLastSlash(__FILE__))
#endif

// Log levels
#define LOG_LEVEL_OFF 1000
#define LOG_LEVEL_ERROR 500
#define LOG_LEVEL_WARN 400
#define LOG_LEVEL_INFO 300
#define LOG_LEVEL_DEBUG 200
#define LOG_LEVEL_TRACE 100
#define LOG_LEVEL_ALL 0

#define LOG_OUTPUT_STREAM stdout

// Default log level
#ifndef LOG_LEVEL
#ifndef NDEBUG
#define LOG_LEVEL LOG_LEVEL_DEBUG
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif
#endif

// For compilers which do not support __FUNCTION__
#if !defined(__FUNCTION__) && !defined(__GNUC__)
#define __FUNCTION__ ""
#endif

// Forward declaration (implementation in namespace DeepXOCR)
namespace DeepXOCR {
    void OutputLogHeader(const char* file, int line, const char* func, int level);
}

// ERROR Level
#ifdef LOG_ERROR_ENABLED
#undef LOG_ERROR_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR_ENABLED
#define LOG_ERROR(...) \
    DeepXOCR::OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_ERROR); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__); \
    fprintf(LOG_OUTPUT_STREAM, "\n"); \
    ::fflush(stdout)
#else
#define LOG_ERROR(...) ((void)0)
#endif

// WARN Level
#ifdef LOG_WARN_ENABLED
#undef LOG_WARN_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN_ENABLED
#define LOG_WARN(...) \
    DeepXOCR::OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_WARN); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__); \
    fprintf(LOG_OUTPUT_STREAM, "\n"); \
    ::fflush(stdout)
#else
#define LOG_WARN(...) ((void)0)
#endif

// INFO Level
#ifdef LOG_INFO_ENABLED
#undef LOG_INFO_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO_ENABLED
#define LOG_INFO(...) \
    DeepXOCR::OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_INFO); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__); \
    fprintf(LOG_OUTPUT_STREAM, "\n"); \
    ::fflush(stdout)
#else
#define LOG_INFO(...) ((void)0)
#endif

// DEBUG Level
#ifdef LOG_DEBUG_ENABLED
#undef LOG_DEBUG_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG_ENABLED
#define LOG_DEBUG(...) \
    DeepXOCR::OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_DEBUG); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__); \
    fprintf(LOG_OUTPUT_STREAM, "\n"); \
    ::fflush(stdout)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

// Modern C++17 helper for debug-only code execution
// Usage: LOG_DEBUG_EXEC([&]{ /* debug code here */ });
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG_EXEC(lambda) lambda()
#else
#define LOG_DEBUG_EXEC(lambda) ((void)0)
#endif

// TRACE Level
#ifdef LOG_TRACE_ENABLED
#undef LOG_TRACE_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE_ENABLED
#define LOG_TRACE(...) \
    DeepXOCR::OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_TRACE); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__); \
    fprintf(LOG_OUTPUT_STREAM, "\n"); \
    ::fflush(stdout)
#else
#define LOG_TRACE(...) ((void)0)
#endif

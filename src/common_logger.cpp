#include "common/logger.hpp"
#include <cstdio>
#include <ctime>

namespace DeepXOCR {

void OutputLogHeader(const char* file, int line, const char* func, int level) {
    const char* level_str;
    const char* color_code;
    
    switch(level) {
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            color_code = "\033[1;31m";  // Red
            break;
        case LOG_LEVEL_WARN:
            level_str = "WARN";
            color_code = "\033[1;33m";  // Yellow
            break;
        case LOG_LEVEL_INFO:
            level_str = "INFO";
            color_code = "\033[1;32m";  // Green
            break;
        case LOG_LEVEL_DEBUG:
            level_str = "DEBUG";
            color_code = "\033[1;36m";  // Cyan
            break;
        case LOG_LEVEL_TRACE:
            level_str = "TRACE";
            color_code = "\033[1;35m";  // Magenta
            break;
        default:
            level_str = "UNKNOWN";
            color_code = "\033[0m";
            break;
    }
    
    // Get current time
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Print log header with color
    fprintf(LOG_OUTPUT_STREAM, "%s[%s] [%s] [%s:%d:%s] ", 
            color_code, time_buffer, level_str, file, line, func);
    fprintf(LOG_OUTPUT_STREAM, "\033[0m");  // Reset color
}

} // namespace DeepXOCR

#include "common/logger.hpp"
#include <cstdio>
#include <ctime>

namespace DeepXOCR {

void OutputLogHeader(const char* file, int line, const char* level, int color) {
    // Get timestamp
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Extract filename from path
    const char* filename = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    
    // Print colored header
    printf("\033[%dm[%s][%s][%s:%d]\033[0m ", color, timestamp, level, filename, line);
}

} // namespace DeepXOCR

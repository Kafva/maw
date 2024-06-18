#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

enum LogLevel {
    MAW_DEBUG,
    MAW_INFO,
    MAW_WARN,
    MAW_ERROR
};

void maw_logf(enum LogLevel level, const char *filename, int line, const char *fmt, ...)
             __attribute__((format (printf, 4, 5)));
void maw_log(enum LogLevel level, const char *filename, int line, const char *msg);
int maw_log_init(bool verbose, int av_log_level);

#define MAW_LOGF(level, fmt, ...) \
    maw_logf(level, __FILE_NAME__, __LINE__, fmt, __VA_ARGS__)

#define MAW_LOG(level, msg) \
    maw_log(level, __FILE_NAME__, __LINE__, msg)

#define MAW_PERROR(msg) do { \
   if (msg != NULL) { \
       MAW_LOGF(MAW_ERROR, "%s: %s\n", msg, strerror(errno)); \
   } else { \
       MAW_LOGF(MAW_ERROR, "%s\n", strerror(errno)); \
   } \
} while (0)

// Log the description of an AV_ERROR
#define MAW_AVERROR(code, msg) do { \
    char errbuf[128] = {0}; \
    if (av_strerror(code, errbuf, sizeof errbuf) != 0) { \
        if (msg != NULL) { \
            MAW_LOGF(MAW_ERROR, "%s: Unknown error\n", msg); \
        } else { \
            MAW_LOG(MAW_ERROR, "Unknown error\n"); \
        } \
    } else { \
        if (msg != NULL) { \
            MAW_LOGF(MAW_ERROR, "%s: %s\n", msg, errbuf); \
        } else { \
            MAW_LOG(MAW_ERROR, errbuf); \
        } \
    } \
} while (0)

#endif // LOG_H

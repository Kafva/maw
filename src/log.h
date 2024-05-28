#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

enum LogLevel {
    MAW_DEBUG,
    MAW_INFO,
    MAW_WARN,
    MAW_ERROR
};

void maw_logf(enum LogLevel, const char *, int, const char *, ...)
             __attribute__((format (printf, 4, 5)));
void maw_log(enum LogLevel, const char *, int, const char *);

int maw_log_init(bool, int);

#define MAW_LOGF(level, fmt, ...) \
    maw_logf(level, __FILE_NAME__, __LINE__, fmt, __VA_ARGS__)

#define MAW_LOG(level, msg) \
    maw_log(level, __FILE_NAME__, __LINE__, msg)

// Log the description of an AV_ERROR
#define MAW_PERROR(code, msg) do { \
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

#ifndef MAW_H
#define MAW_H

#include <stdbool.h>

struct Metadata {
    char *title;
    char *album;
    char *artist;
    char *cover_path;
    bool clear_metadata;
};

int maw_yaml_parse(const char *);

void maw_logf(int, const char *, int, const char *, ...)
             __attribute__((format (printf, 4, 5)));
void maw_log(int, const char *, int, const char *);

int maw_init(int);
int maw_dump(const char *);
int maw_update(const char *, const struct Metadata *);

#define MAW_LOGF(level, fmt, ...) \
    maw_logf(level, __FILE_NAME__, __LINE__, fmt, __VA_ARGS__)

#define MAW_LOG(level, msg) \
    maw_log(level, __FILE_NAME__, __LINE__, msg)

// Log the description of an AV_ERROR
#define MAW_PERROR(code, msg) do { \
    char errbuf[128] = {0}; \
    if (av_strerror(code, errbuf, sizeof errbuf) != 0) { \
        if (msg != NULL) { \
            MAW_LOGF(AV_LOG_ERROR, "%s: Unknown error\n", msg); \
        } else { \
            MAW_LOG(AV_LOG_ERROR, "Unknown error\n"); \
        } \
    } else { \
        if (msg != NULL) { \
            MAW_LOGF(AV_LOG_ERROR, "%s: %s\n", msg, errbuf); \
        } else { \
            MAW_LOG(AV_LOG_ERROR, errbuf); \
        } \
    } \
} while (0)

#endif // MAW_H

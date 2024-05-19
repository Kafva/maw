#include "log.h"

#include <libavutil/log.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

static bool maw_verbose = false;

static void maw_log_prefix(int level, const char *filename, int line) {
    switch (level) {
        case AV_LOG_DEBUG:
            fprintf(stderr, "\033[94mDEBUG\033[0m [%s:%d] ", filename, line);
            break;
        case AV_LOG_INFO:
            fprintf(stderr, "\033[92mINFO\033[0m [%s:%d] ", filename, line);
            break;
        case AV_LOG_WARNING:
            fprintf(stderr, "\033[93mWARN\033[0m [%s:%d] ", filename, line);
            break;
        case AV_LOG_ERROR:
            fprintf(stderr, "\033[91mERROR\033[0m [%s:%d] ", filename, line);
            break;
    }
}

void maw_logf(int level, const char *filename, int line, const char *fmt, ...) {
    va_list args;

    if (level == AV_LOG_DEBUG && !maw_verbose) {
        return;
    }
    maw_log_prefix(level, filename, line);

    va_start(args, fmt);

    vfprintf(stderr, fmt, args);

    va_end(args);
}

void maw_log(int level, const char *filename, int line, const char *msg) {
    if (level == AV_LOG_DEBUG && !maw_verbose) {
        return;
    }
    maw_log_prefix(level, filename, line);
    write(STDERR_FILENO, msg, strlen(msg));
}

int maw_log_init(bool verbose, int av_log_level) {
    maw_verbose = verbose;
    av_log_set_level(av_log_level);
    return 0;
}


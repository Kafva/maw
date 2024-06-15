#include "log.h"

#include <libavutil/log.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

static bool maw_verbose = false;
static bool enable_color = true;

static void maw_log_prefix(enum LogLevel level, const char *filename, int line) {
    switch (level) {
        case MAW_DEBUG:
            if (enable_color)
                fprintf(stderr, "\033[94mDEBUG\033[0m [%s:%d] ", filename, line);
            else
                fprintf(stderr, "DEBUG [%s:%d] ", filename, line);
            break;
        case MAW_INFO:
            if (enable_color)
                fprintf(stderr, "\033[92mINFO\033[0m [%s:%d] ", filename, line);
            else
                fprintf(stderr, "INFO [%s:%d] ", filename, line);
            break;
        case MAW_WARN:
            if (enable_color)
                fprintf(stderr, "\033[93mWARN\033[0m [%s:%d] ", filename, line);
            else
                fprintf(stderr, "WARN [%s:%d] ", filename, line);
            break;
        case MAW_ERROR:
            if (enable_color)
                fprintf(stderr, "\033[91mERROR\033[0m [%s:%d] ", filename, line);
            else
                fprintf(stderr, "ERROR [%s:%d] ", filename, line);
            break;
    }
}

void maw_logf(enum LogLevel level, const char *filename, int line, const char *fmt, ...) {
    va_list args;

#ifdef MAW_TEST
    // Be completely silent during tests unless we pass '-v'
    if (!maw_verbose) {
        return;
    }
#endif
    if (level == MAW_DEBUG && !maw_verbose) {
        return;
    }
    maw_log_prefix(level, filename, line);

    va_start(args, fmt);

    vfprintf(stderr, fmt, args);

    va_end(args);
}

void maw_log(enum LogLevel level, const char *filename, int line, const char *msg) {
    if (level == MAW_DEBUG && !maw_verbose) {
        return;
    }
    maw_log_prefix(level, filename, line);
    (void)write(STDERR_FILENO, msg, strlen(msg));
}

int maw_log_init(bool verbose, int av_log_level) {
    maw_verbose = verbose;
    enable_color = isatty(fileno(stdout)) && isatty(fileno(stderr));
    av_log_set_level(av_log_level);
    return 0;
}

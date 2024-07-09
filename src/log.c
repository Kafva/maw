#include "maw/log.h"

#include <libavutil/log.h>

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>

static bool maw_verbose = false;
static bool maw_is_tty = true;

static void maw_log_prefix(enum LogLevel level, const char *filename, int line,
                           char *out, size_t outsize) {
    const char *fmt_str;
    switch (level) {
    case MAW_DEBUG:
        fmt_str =
            maw_is_tty ? "\033[94mDEBUG\033[0m [%s:%d] " : "DEBUG [%s:%d] ";
        break;
    case MAW_INFO:
        fmt_str = maw_is_tty ? "\033[92mINFO\033[0m [%s:%d] " : "INFO [%s:%d] ";
        break;
    case MAW_WARN:
        fmt_str = maw_is_tty ? "\033[93mWARN\033[0m [%s:%d] " : "WARN [%s:%d] ";
        break;
    case MAW_ERROR:
        fmt_str =
            maw_is_tty ? "\033[91mERROR\033[0m [%s:%d] " : "ERROR [%s:%d] ";
        break;
    default:
        fmt_str = "[%s:%d] ";
    }
    (void)snprintf(out, outsize, fmt_str, filename, line);
}

// Newline is automatically added to the end of the message
void maw_logf(enum LogLevel level, const char *filename, int line,
              const char *fmt, ...) {
    char fmt_full[MAW_LOG_MAX_MSGSIZE];
    va_list args;

#ifdef MAW_TEST
    // Be completely silent during tests unless we pass '-v'
    if (!maw_verbose) {
        return;
    }
#else
    if (level == MAW_DEBUG && !maw_verbose) {
        return;
    }
#endif

    va_start(args, fmt);

    maw_log_prefix(level, filename, line, fmt_full, sizeof fmt_full);

    // Print the entire output string as one unit to avoid
    // overlapping partial messages when running multiple threads
    (void)strlcat(fmt_full, fmt, sizeof fmt_full);
    (void)strlcat(fmt_full, "\n", sizeof fmt_full);

    (void)vfprintf(MAW_LOG_FP, fmt_full, args);

    va_end(args);
}

// Newline is automatically added to the end of the message
void maw_log(enum LogLevel level, const char *filename, int line,
             const char *msg) {
    char fmt_full[MAW_LOG_MAX_MSGSIZE];

#ifdef MAW_TEST
    // Be completely silent during tests unless we pass '-v'
    if (!maw_verbose) {
        return;
    }
#else
    if (level == MAW_DEBUG && !maw_verbose) {
        return;
    }
#endif

    maw_log_prefix(level, filename, line, fmt_full, sizeof fmt_full);
    (void)strlcat(fmt_full, msg, sizeof fmt_full);
    (void)strlcat(fmt_full, "\n", sizeof fmt_full);

    (void)write(fileno(MAW_LOG_FP), fmt_full, strlen(fmt_full));
    (void)fflush(MAW_LOG_FP);
}

void maw_log_init(bool verbose, int av_log_level) {
    maw_verbose = verbose;
    maw_is_tty = isatty(fileno(stdout)) && isatty(fileno(stderr));

    av_log_set_level(av_log_level);
}

#include "maw/log.h"

#include <libavutil/log.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

static bool maw_verbose = false;
static bool maw_simple_log = true;
static bool maw_is_tty = true;
static const char *logfile = "/tmp/.maw.log";
static const char *av_logfile = "/tmp/.av_maw.log";
static FILE *lfp = NULL;
static FILE *av_lfp = NULL;

static void maw_log_msg_end(void) {
    if (maw_simple_log) {
        (void)write(STDERR_FILENO, "\n", 1);
    }
    else {
        (void)write(fileno(lfp), "\n", 1);
        (void)write(STDERR_FILENO, "\r", 1);
    }
}

static void maw_av_log_callback(void *ptr, int level, const char *fmt, va_list vargs) {
    int r;
    if (level > av_log_get_level())
        return;

    r = vfprintf(av_lfp, fmt, vargs);
    if (r < 0) {
        fprintf(stderr, "vfprintf failed: %d\n", r);
    }

    maw_log_msg_end();
}

static void maw_log_prefix(FILE* fp, enum LogLevel level, const char *filename, int line) {
    switch (level) {
        case MAW_DEBUG:
            if (maw_is_tty)
                fprintf(fp, "\033[94mDEBUG\033[0m [%s:%d] ", filename, line);
            else
                fprintf(fp, "DEBUG [%s:%d] ", filename, line);
            break;
        case MAW_INFO:
            if (maw_is_tty)
                fprintf(fp, "\033[92mINFO\033[0m [%s:%d] ", filename, line);
            else
                fprintf(fp, "INFO [%s:%d] ", filename, line);
            break;
        case MAW_WARN:
            if (maw_is_tty)
                fprintf(fp, "\033[93mWARN\033[0m [%s:%d] ", filename, line);
            else
                fprintf(fp, "WARN [%s:%d] ", filename, line);
            break;
        case MAW_ERROR:
            if (maw_is_tty)
                fprintf(fp, "\033[91mERROR\033[0m [%s:%d] ", filename, line);
            else
                fprintf(fp, "ERROR [%s:%d] ", filename, line);
            break;
    }
}

void maw_logf(enum LogLevel level, const char *filename, int line, const char *fmt, ...) {
    int r;
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

    va_start(args, fmt);

    if (!maw_simple_log) {
        maw_log_prefix(lfp, level, filename, line);
        (void)vfprintf(lfp, fmt, args);
    }

    maw_log_prefix(stderr, level, filename, line);
    r = vfprintf(stderr, fmt, args);
    if (r < 0) {
        fprintf(stderr, "vfprintf failed: %d\n", r);
    }

    maw_log_msg_end();

    va_end(args);
}

void maw_log(enum LogLevel level, const char *filename, int line, const char *msg) {
    if (level == MAW_DEBUG && !maw_verbose) {
        return;
    }

    if (!maw_simple_log) {
        maw_log_prefix(lfp, level, filename, line);
        (void)write(fileno(lfp), msg, strlen(msg));
    }

    maw_log_prefix(stderr, level, filename, line);
    (void)write(STDERR_FILENO, msg, strlen(msg));

    maw_log_msg_end();
}


int maw_log_init(bool verbose, bool simple_log, int av_log_level) {
    maw_verbose = verbose;
    maw_simple_log = simple_log;
    maw_is_tty = isatty(fileno(stdout)) && isatty(fileno(stderr));

    av_log_set_level(av_log_level);

    if (!maw_simple_log) {
        av_log_set_callback(maw_av_log_callback);

        // Save the complete log in a logfile so that we can show a trace
        // when an error occurs
        lfp = fopen(logfile, "w");
        if (lfp == NULL) {
            fprintf(stderr, "Failed to open internal log file: %s", logfile);
            return 1;
        }

        av_lfp = fopen(av_logfile, "w");
        if (lfp == NULL) {
            fprintf(stderr, "Failed to open internal libav log file: %s", av_logfile);
            return 1;
        }
    }

    return 0;
}

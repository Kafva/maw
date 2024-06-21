#include "log.h"

#include <libavutil/log.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

static bool maw_verbose = false;
static bool maw_simple_log = true;
static bool is_tty = true;
static const char *logfile = "/tmp/.maw.log";
static FILE *lfp = NULL;

static void maw_log_msg_end(void) {
    if ((maw_simple_log || !is_tty) && fileno(lfp) != STDERR_FILENO) {
        (void)write(fileno(lfp), "\n", 1);
    }
    else {
        (void)write(STDERR_FILENO, "\r", 1);
    }
}

static void maw_log_prefix(enum LogLevel level, const char *filename, int line) {
    switch (level) {
        case MAW_DEBUG:
            if (is_tty)
                fprintf(lfp, "\033[94mDEBUG\033[0m [%s:%d] ", filename, line);
            else
                fprintf(lfp, "DEBUG [%s:%d] ", filename, line);
            break;
        case MAW_INFO:
            if (is_tty)
                fprintf(lfp, "\033[92mINFO\033[0m [%s:%d] ", filename, line);
            else
                fprintf(lfp, "INFO [%s:%d] ", filename, line);
            break;
        case MAW_WARN:
            if (is_tty)
                fprintf(lfp, "\033[93mWARN\033[0m [%s:%d] ", filename, line);
            else
                fprintf(lfp, "WARN [%s:%d] ", filename, line);
            break;
        case MAW_ERROR:
            if (is_tty)
                fprintf(lfp, "\033[91mERROR\033[0m [%s:%d] ", filename, line);
            else
                fprintf(lfp, "ERROR [%s:%d] ", filename, line);
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

    vfprintf(lfp, fmt, args);

    maw_log_msg_end();

    va_end(args);
}

void maw_log(enum LogLevel level, const char *filename, int line, const char *msg) {
    if (level == MAW_DEBUG && !maw_verbose) {
        return;
    }
    maw_log_prefix(level, filename, line);
    (void)write(fileno(lfp), msg, strlen(msg));

    if ((maw_simple_log || !is_tty) && fileno(lfp) != STDERR_FILENO) {
        (void)write(STDERR_FILENO, msg, strlen(msg));
    }

    maw_log_msg_end();
}


int maw_log_init(bool verbose, bool simple_log, int av_log_level) {
    maw_verbose = verbose;
    maw_simple_log = simple_log;
    is_tty = isatty(fileno(stdout)) && isatty(fileno(stderr));
    av_log_set_level(av_log_level);

    if (simple_log || !is_tty) {
        lfp = stderr;
    }
    else {
        // Save the complete log in a logfile so that we can show a trace
        // when an error occurs
        lfp = fopen(logfile, "w");
        if (lfp == NULL) {
            fprintf(stderr, "Failed to open internal log file: %s", logfile);
            return 1;
        }
    }

    return 0;
}

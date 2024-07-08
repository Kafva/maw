#include "maw/log.h"
#include "maw/maw.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libavutil/error.h>
#include <libavutil/log.h>

#ifndef MAW_PROGRAM
#define MAW_PROGRAM "maw"
#endif

#define _MAW_OPTS "c:j:l:hv"

#ifdef MAW_TEST
#include "maw/tests/maw_test.h"
#define MAW_OPTS "m:" _MAW_OPTS
#else
#include "maw/cfg.h"
#define MAW_OPTS _MAW_OPTS
static int run_program(const char *);
#endif

static void usage(void);

static const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"jobs", optional_argument, NULL, 'j'},
    {"verbose", no_argument, NULL, 'v'},
    {"log", optional_argument, NULL, 'l'},
#ifdef MAW_TEST
    {"match", optional_argument, NULL, 'm'},
#endif
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

static const char *long_options_usage[] = {"YAML configuration file to use",
                                           "Number of parrallel jobs to run",
                                           "Verbose logging",
                                           "Log level for libav backend",
#ifdef MAW_TEST
                                           "Testcase to run",
#endif
                                           "Show this help message",
                                           NULL};

int main(int argc, char *argv[]) {
    int opt;
    int av_log_level = AV_LOG_QUIET;
    bool verbose = false;
    char *config_file = NULL;
#ifdef MAW_TEST
    const char *match_testcase = NULL;
#endif

    while ((opt = getopt_long(argc, argv, MAW_OPTS, long_options, NULL)) !=
           -1) {
        switch (opt) {
        case 'c':
            config_file = optarg;
            break;
#ifdef MAW_TEST
        case 'm':
            match_testcase = optarg;
            break;
#endif
        case 'v':
            verbose = true;
            break;
        case 'l':
            if (STR_CASE_MATCH("debug", optarg)) {
                av_log_level = AV_LOG_DEBUG;
            }
            else if (STR_CASE_MATCH("warning", optarg)) {
                av_log_level = AV_LOG_WARNING;
            }
            else if (STR_CASE_MATCH("info", optarg)) {
                av_log_level = AV_LOG_INFO;
            }
            else if (STR_CASE_MATCH("error", optarg)) {
                av_log_level = AV_LOG_ERROR;
            }
            else if (STR_CASE_MATCH("quiet", optarg)) {
                av_log_level = AV_LOG_QUIET;
            }
            else {
                fprintf(stderr, "Invalid log level\n");
                return EXIT_FAILURE;
            }
            break;
        default:
            usage();
            return EXIT_FAILURE;
        }
    }

    maw_log_init(verbose, av_log_level);

#ifdef MAW_TEST
    (void)config_file;
    return run_tests(match_testcase);
#else
    return run_program(config_file);
#endif
}

static void usage(void) {
    size_t optcount = (sizeof(long_options) / sizeof(struct option)) - 1;
    char *arg = NULL;

    ASSERT((sizeof(long_options) / sizeof(struct option)) ==
           sizeof(long_options_usage) / sizeof(char *));

    fprintf(stderr, "usage: " MAW_PROGRAM " [flags] <cmd>\n\n");
    fprintf(stderr, "COMMANDS: \n");
    fprintf(stderr, "    up [path]            Update metadata according to "
                    "config, optionally limited to [path]\n");
    fprintf(stderr, "    generate             Generate playlists\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "OPTIONS: \n");

    for (size_t i = 0; i < optcount; i++) {
        arg = long_options[i].has_arg ? " <arg> " : "       ";
        fprintf(stderr, "    --%-18s%s%-30s\n", long_options[i].name, arg,
                long_options_usage[i]);
    }
}

#ifndef MAW_TEST

static int run_program(const char *config_file) {
    int r = EXIT_FAILURE;
    MawConfig *cfg = NULL;

    if (config_file == NULL) {
        fprintf(stderr, "No config file provided\n");
        usage();
        return EXIT_FAILURE;
    }

    r = maw_cfg_parse(config_file, &cfg);
    if (r != 0) {
        goto end;
    }

    r = EXIT_SUCCESS;
end:
    return r;
}

#endif

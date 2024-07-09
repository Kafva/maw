#include "maw/log.h"
#include "maw/maw.h"
#include "maw/threads.h"

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

#define HEADER_COLOR "\033[33m"
#define OPT_COLOR    "\033[32m"
#define NO_COLOR     "\033[0m"

#define _MAW_OPTS "c:j:l:hv"

#ifdef MAW_TEST
#include "maw/tests/maw_test.h"
#define MAW_OPTS "m:" _MAW_OPTS
#else
#include "maw/cfg.h"
#define MAW_OPTS _MAW_OPTS
static int run_program(int argc, char *argv[], const char *config_file,
                       ssize_t thread_count);

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

// clang-format off
static const char *long_options_usage[] = {
    "YAML configuration file to use",
    "Number of parrallel jobs to run",
    "Verbose logging",
    "Log level for libav backend",
#ifdef MAW_TEST
    "Testcase to run",
#endif
    "Show this help message",
    NULL};
// clang-format on

int main(int argc, char *argv[]) {
    int opt;
    int av_log_level = AV_LOG_QUIET;
    bool verbose = false;
    ssize_t thread_count = 1;
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
        case 'j':
            thread_count = strtol(optarg, NULL, 10);
            if (thread_count <= 0) {
                fprintf(stderr, "Invalid argument for job count: %s\n", optarg);
                return EXIT_FAILURE;
            }
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
    return run_program(argc, argv, config_file, (ssize_t)thread_count);
#endif
}

static void usage(void) {
    size_t optcount = (sizeof(long_options) / sizeof(struct option)) - 1;
    char buf[1024];

    ASSERT((sizeof(long_options) / sizeof(struct option)) ==
           sizeof(long_options_usage) / sizeof(char *));

    // clang-format off
    fprintf(stderr, HEADER_COLOR"USAGE:"NO_COLOR"\n");
    fprintf(stderr, MAW_PROGRAM " [OPTIONS] <COMMAND>\n\n");
    fprintf(stderr, HEADER_COLOR"COMMANDS:"NO_COLOR"\n");
    fprintf(stderr, OPT_COLOR"    up [paths]"NO_COLOR"            Update metadata in [paths] according to config\n");
    fprintf(stderr, OPT_COLOR"    generate"NO_COLOR"              Generate playlists\n");
    fprintf(stderr, "\n");
    fprintf(stderr, HEADER_COLOR"OPTIONS:"NO_COLOR"\n");
    // clang-format on

    for (size_t i = 0; i < optcount; i++) {

        (void)strlcpy(buf, long_options[i].name, sizeof buf);
        if (long_options[i].has_arg) {
            (void)strlcat(buf, " <arg>", sizeof buf);
        }
        fprintf(stderr, OPT_COLOR "    -%c, %-18s" NO_COLOR "%-30s\n",
                long_options[i].val, buf, long_options_usage[i]);
    }
}

#ifndef MAW_TEST

static int run_program(int argc, char *argv[], const char *config_file,
                       ssize_t thread_count) {
    int r = EXIT_FAILURE;
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    ssize_t mediafiles_count = 0;

    if (config_file == NULL) {
        fprintf(stderr, "No config file provided\n");
        return EXIT_FAILURE;
    }

    r = maw_cfg_parse(config_file, &cfg);
    if (r != 0)
        goto end;

    r = maw_cfg_alloc_mediafiles(cfg, mediafiles, &mediafiles_count);
    if (r != 0)
        goto end;

    r = maw_gen_playlists(cfg);
    if (r != 0)
        goto end;

    // TODO limit to path
    r = maw_threads_launch(mediafiles, mediafiles_count, (size_t)thread_count);
    if (r != 0)
        goto end;

    r = EXIT_SUCCESS;
end:
    maw_cfg_free(cfg);
    maw_mediafiles_free(mediafiles, mediafiles_count);
    return r;
}

#endif

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

struct MawArguments {
    char *config_file;
    size_t thread_count;
    bool verbose;
    int av_log_level;
#ifdef MAW_TEST
    char *match_testcase;
#endif
    char *cmd;
    char **cmd_args;
    int cmd_args_count;
} typedef MawArguments;

#ifdef MAW_TEST
#include "maw/tests/maw_test.h"
#define MAW_OPTS "m:" _MAW_OPTS
#else
#include "maw/cfg.h"
#include "maw/playlists.h"
#include "maw/update.h"
#define MAW_OPTS _MAW_OPTS
static int run_update(MawArguments args, MawConfig *cfg);
static int run_program(MawArguments args);

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
    ssize_t thread_count;
    // clang-format off
    MawArguments args = {
        .config_file = NULL,
        .verbose = false,
        .thread_count = 1,
        .av_log_level = AV_LOG_QUIET,
#ifdef MAW_TEST
        .match_testcase = NULL,
#endif
        .cmd = NULL,
        .cmd_args = NULL,
        .cmd_args_count = 0
    };
    // clang-format on

    while ((opt = getopt_long(argc, argv, MAW_OPTS, long_options, NULL)) !=
           -1) {
        switch (opt) {
        case 'c':
            args.config_file = optarg;
            break;
#ifdef MAW_TEST
        case 'm':
            args.match_testcase = optarg;
            break;
#endif
        case 'v':
            args.verbose = true;
            break;
        case 'j':
            thread_count = strtol(optarg, NULL, 10);
            if (thread_count <= 0) {
                fprintf(stderr, "Invalid argument for job count: %s\n", optarg);
                return EXIT_FAILURE;
            }
            args.thread_count = (size_t)thread_count;
            break;
        case 'l':
            if (STR_CASE_MATCH("debug", optarg)) {
                args.av_log_level = AV_LOG_DEBUG;
            }
            else if (STR_CASE_MATCH("warning", optarg)) {
                args.av_log_level = AV_LOG_WARNING;
            }
            else if (STR_CASE_MATCH("info", optarg)) {
                args.av_log_level = AV_LOG_INFO;
            }
            else if (STR_CASE_MATCH("error", optarg)) {
                args.av_log_level = AV_LOG_ERROR;
            }
            else if (STR_CASE_MATCH("quiet", optarg)) {
                args.av_log_level = AV_LOG_QUIET;
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

    // Shift out parsed options from argv
    argc -= optind;
    argv += optind;

    args.cmd = argv[0];
    if (argc > 0) {
        args.cmd_args = argv + 1;
        args.cmd_args_count = argc - 1;
    }

    maw_log_init(args.verbose, args.av_log_level);

#ifdef MAW_TEST
    return run_tests(args.match_testcase);
#else
    return run_program(args);
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

static int run_update(MawArguments args, MawConfig *cfg) {
    int r = EXIT_FAILURE;
    MediaFile mediafiles[MAW_MAX_FILES];
    ssize_t mediafiles_count = 0;

    r = maw_cfg_mediafiles_alloc(cfg, mediafiles, &mediafiles_count);
    if (r != 0)
        goto end;

    r = maw_threads_launch(mediafiles, mediafiles_count, args.thread_count);
    if (r != 0)
        goto end;

    r = 0;

end:
    maw_cfg_mediafiles_free(mediafiles, mediafiles_count);
    return r;
}

static int run_program(MawArguments args) {
    int r = EXIT_FAILURE;
    MawConfig *cfg = NULL;

    if (args.config_file == NULL) {
        fprintf(stderr, "No config file provided\n");
        return EXIT_FAILURE;
    }

    if (STR_MATCH("gen", args.cmd) || STR_MATCH("generate", args.cmd)) {
        r = maw_cfg_parse(args.config_file, &cfg);
        if (r != 0)
            goto end;
        r = maw_playlists_gen(cfg);
        if (r != 0)
            goto end;
    }
    else if (STR_MATCH("up", args.cmd) || STR_MATCH("update", args.cmd)) {
        r = maw_cfg_parse(args.config_file, &cfg);
        if (r != 0)
            goto end;
        r = run_update(args, cfg);
        if (r != 0)
            goto end;
    }
    else {
        fprintf(stderr, "Unknown command: '%s'\n", args.cmd);
        goto end;
    }

    r = EXIT_SUCCESS;
end:
    maw_cfg_free(cfg);
    return r;
}

#endif

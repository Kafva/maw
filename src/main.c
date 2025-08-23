#include "maw/log.h"
#include "maw/threads.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libavutil/error.h>
#include <libavutil/log.h>

#ifndef MAW_PROGRAM
#define MAW_PROGRAM "maw"
#endif

#ifndef MAW_VERSION
#define MAW_VERSION "unknown"
#endif

#ifdef DEBUG
#define MAW_BUILDTYPE "debug"
#else
#define MAW_BUILDTYPE "release"
#endif

#define HEADER_COLOR "\033[1;4m"
#define OPT_COLOR    "\033[1m"
#define NO_COLOR     "\033[0m"

#define _MAW_OPTS "c:j:l:hvn"

#ifdef MAW_TEST
#include "maw/tests/maw_test.h"
#define MAW_OPTS "m:" _MAW_OPTS
#else
#include "maw/cfg.h"
#include "maw/playlists.h"
#include "maw/update.h"
#define MAW_OPTS _MAW_OPTS
static int set_config(MawArguments *args, char *config_path, size_t size);
static int run_update(MawArguments *args, MawConfig *cfg);
static int run_program(MawArguments *args);

#endif

static void usage(void);

static const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"jobs", optional_argument, NULL, 'j'},
    {"verbose", no_argument, NULL, 'v'},
    {"dry-run", no_argument, NULL, 'n'},
    {"log", optional_argument, NULL, 'l'},
#ifdef MAW_TEST
    {"match", optional_argument, NULL, 'm'},
#endif
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

// clang-format off
static const char *long_options_usage[] = {
    "YAML configuration file to use",
    "Number of parallel jobs to run",
    "Verbose logging",
    "Do not make any changes to media files",
    "Log level for libav*",
#ifdef MAW_TEST
    "Testcase to run",
#endif
    "Show this help message",
    NULL};
// clang-format on

int main(int argc, char *argv[]) {
    int opt;
    size_t thread_count;
    // clang-format off
    MawArguments args = {
        .config_path = NULL,
        .verbose = false,
        .dry_run = false,
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
            args.config_path = optarg;
            break;
#ifdef MAW_TEST
        case 'm':
            args.match_testcase = optarg;
            break;
#endif
        case 'v':
            args.verbose = true;
            break;
        case 'n':
            args.dry_run = true;
            break;
        case 'j':
            thread_count = strtoul(optarg, NULL, 10);
            if (thread_count <= 0) {
                printf("Invalid argument for job count: %s\n", optarg);
                return EXIT_FAILURE;
            }
            args.thread_count = (size_t)thread_count;
            break;
        case 'l':
            if (STR_CASE_EQ("debug", optarg)) {
                args.av_log_level = AV_LOG_DEBUG;
            }
            else if (STR_CASE_EQ("warning", optarg)) {
                args.av_log_level = AV_LOG_WARNING;
            }
            else if (STR_CASE_EQ("info", optarg)) {
                args.av_log_level = AV_LOG_INFO;
            }
            else if (STR_CASE_EQ("error", optarg)) {
                args.av_log_level = AV_LOG_ERROR;
            }
            else if (STR_CASE_EQ("quiet", optarg)) {
                args.av_log_level = AV_LOG_QUIET;
            }
            else {
                printf("Invalid log level\n");
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
    return run_program(&args);
#endif
}

static void usage(void) {
    size_t optcount = (sizeof(long_options) / sizeof(struct option)) - 1;
    char buf[1024];

    ASSERT((sizeof(long_options) / sizeof(struct option)) ==
           sizeof(long_options_usage) / sizeof(char *));

    // clang-format off
    if (strlen(MAW_VERSION) > /* DISABLES CODE */ (0)) {
        printf(OPT_COLOR MAW_PROGRAM NO_COLOR " " MAW_VERSION " [" MAW_BUILDTYPE "]\n");
    }
    else {
        printf(OPT_COLOR MAW_PROGRAM NO_COLOR " [" MAW_BUILDTYPE "]\n");
    }
    printf("\n");
    printf(HEADER_COLOR"USAGE:"NO_COLOR"\n");
    printf(MAW_PROGRAM " [OPTIONS] <COMMAND>\n\n");
    printf(HEADER_COLOR"COMMANDS:"NO_COLOR"\n");
    printf(OPT_COLOR"    update [paths]"NO_COLOR"          Update metadata in [paths] according to config\n");
    printf(OPT_COLOR"    generate"NO_COLOR"                Generate playlists\n");
    printf("\n");
    printf(HEADER_COLOR"OPTIONS:"NO_COLOR"\n");
    // clang-format on

    for (size_t i = 0; i < optcount; i++) {
        (void)strlcpy(buf, long_options[i].name, sizeof buf);
        if (long_options[i].has_arg) {
            (void)strlcat(buf, " <arg>", sizeof buf);
        }
        printf(OPT_COLOR "    -%c, --%-18s" NO_COLOR "%-30s\n",
               long_options[i].val, buf, long_options_usage[i]);
    }
}

#ifndef MAW_TEST

static int set_config(MawArguments *args, char *config_path, size_t size) {
    int r = RESULT_ERR_INTERNAL;
    struct stat s;
    char *envvar = NULL;

    // Use provided configuration
    if (args->config_path != NULL) {
        MAW_STRLCPY_SIZE(config_path, args->config_path, size);
        r = RESULT_OK;
        goto end;
    }

    envvar = getenv("XDG_CONFIG_HOME");
    if (envvar == NULL) {
        envvar = getenv("HOME");
        if (envvar == NULL) {
            MAW_LOG(MAW_ERROR, "HOME is unset");
            goto end;
        }
        MAW_STRLCPY_SIZE(config_path, envvar, size);
        MAW_STRLCAT_SIZE(config_path, "/.config/maw/maw.yml", size);
    }
    else {
        MAW_STRLCPY_SIZE(config_path, envvar, size);
        MAW_STRLCAT_SIZE(config_path, "/maw/maw.yml", size);
    }

    if (stat(config_path, &s) < 0) {
        if (errno == ENOENT) {
            printf("No config file provided\n");
        }
        else {
            MAW_PERRORF("stat", config_path);
        }
        goto end;
    }

    r = RESULT_OK;
end:
    return r;
}

static int run_update(MawArguments *args, MawConfig *cfg) {
    int r = EXIT_FAILURE;
    MediaFile mediafiles[MAW_MAX_FILES];
    size_t mediafiles_count = 0;

    r = maw_update_load(cfg, args, mediafiles, &mediafiles_count);
    if (r != 0)
        goto end;

    if (mediafiles_count == 0) {
        printf("No media files matched\n");
        fflush(stderr);
        goto end;
    }

    if (args->dry_run) {
        maw_update_dump(mediafiles, mediafiles_count);
    }

    r = maw_threads_launch(mediafiles, mediafiles_count, args->thread_count,
                           args->dry_run);
    if (r != 0)
        goto end;

    r = RESULT_OK;

end:
    maw_update_free(mediafiles, mediafiles_count);
    return r;
}

static int run_program(MawArguments *args) {
    int r = EXIT_FAILURE;
    MawConfig *cfg = NULL;
    char config_path[MAW_PATH_MAX];

    r = set_config(args, config_path, sizeof config_path);
    if (r != 0) {
        usage();
        return EXIT_FAILURE;
    }
    if (args->cmd == NULL) {
        printf("No command provided\n");
        usage();
        return EXIT_FAILURE;
    }

    if (STR_EQ("gen", args->cmd) || STR_EQ("generate", args->cmd)) {
        r = maw_cfg_parse(config_path, &cfg);
        if (r != 0)
            goto end;
        r = maw_playlists_gen(cfg);
        if (r != 0)
            goto end;
    }
    else if (STR_EQ("up", args->cmd) || STR_EQ("update", args->cmd)) {
        r = maw_cfg_parse(config_path, &cfg);
        if (r != 0)
            goto end;

        r = run_update(args, cfg);
        if (r != 0)
            goto end;
    }
    else {
        printf("Unknown command: '%s'\n", args->cmd);
        goto end;
    }

    r = EXIT_SUCCESS;
end:
    maw_cfg_free(cfg);
    return r;
}

#endif

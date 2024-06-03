#include "maw.h"
#include "log.h"
#include "cfg.h"

#include <getopt.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>

#include <libavutil/log.h>
#include <libavutil/error.h>

#ifndef MAW_PROGRAM
#define MAW_PROGRAM "maw"
#endif

#ifdef MAW_TEST
#include "tests/maw_test.h"
static int run_tests(const char *);
#endif

static void usage(void);

static void usage(void) {
    fprintf(stderr, "usage: " MAW_PROGRAM " [flags]\n");
    fprintf(stderr, "   --verbose        Verbose logging\n");
    fprintf(stderr, "   --log <level>    Log level for libav backend\n");
    fprintf(stderr, "   --help           Show this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;
    int av_log_level = AV_LOG_QUIET;
    bool verbose = false;
    char *config_file = NULL;
#ifdef MAW_TEST
    const char *getopt_flags = "m:c:l:hv";
    const char *match_testcase = NULL; 
#else
    const char *getopt_flags = "c:l:hv";
#endif

    static struct option long_options[] = {
        {"log", optional_argument, NULL, 'l'},
        {"verbose", no_argument, NULL, 'v'},
#ifdef MAW_TEST
        {"match", optional_argument, NULL, 'm'},
#endif
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, getopt_flags, long_options, NULL)) != -1) {
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
            if (strncasecmp("debug", optarg, sizeof("debug") - 1) == 0) {
                av_log_level = AV_LOG_DEBUG;
            }
            else if (strncasecmp("warning", optarg, sizeof("warning") - 1) == 0) {
                av_log_level = AV_LOG_WARNING;
            }
            else if (strncasecmp("info", optarg, sizeof("info") - 1) == 0) {
                av_log_level = AV_LOG_INFO;
            }
            else if (strncasecmp("error", optarg, sizeof("error") - 1) == 0) {
                av_log_level = AV_LOG_ERROR;
            }
            else if (strncasecmp("quiet", optarg, sizeof("quiet") - 1) == 0) {
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
    return run_tests(match_testcase);
#else
    if (config_file == NULL) {
        MAW_LOG(MAW_ERROR, "Missing required options\n");
        usage();
        return EXIT_FAILURE;
    }

    (void)maw_cfg_parse(config_file);
    return EXIT_SUCCESS;

#endif
}

static int run_tests(const char *match_testcase) {
    DEFINE_TESTCASES;
    int total = sizeof(testcases) / sizeof(struct Testcase);
    int i;
    int r;
    bool enable_color = isatty(fileno(stdout)) && isatty(fileno(stderr));

    fprintf(stdout, "0..%d\n", total - 1);
    for (i = 0; i < total; i++) {

        if (match_testcase != NULL) {
            r = strncmp(match_testcase, testcases[i].desc, strlen(match_testcase));
            if (r != 0) {
                if (enable_color)
                    fprintf(stdout, "\033[38;5;246mok\033[0m %d - %s # skip\n", i, testcases[i].desc);
                else
                    fprintf(stdout, "ok %d - %s # skip\n", i, testcases[i].desc);
                continue;
            }
        }

        if (testcases[i].fn()) {
            if (enable_color)
                fprintf(stdout, "\033[92mok\033[0m %d - %s\n", i, testcases[i].desc);
            else
                fprintf(stdout, "ok %d - %s\n", i, testcases[i].desc);
        } else {
            if (enable_color)
                fprintf(stdout, "\033[91mnot ok\033[0m %d - %s\n", i, testcases[i].desc);
            else
                fprintf(stdout, "not ok %d - %s\n", i, testcases[i].desc);
            return EXIT_FAILURE; // XXX
        }
    }

    return EXIT_SUCCESS;
}

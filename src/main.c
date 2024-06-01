#include "maw.h"
#include "log.h"

#include <getopt.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>

#include <libavutil/log.h>
#include <libavutil/error.h>

#define PROGRAM "maw"

#ifndef MAW_TEST

static void usage(void);

static void usage(void) {
    fprintf(stderr, "usage: " PROGRAM " [flags]\n");
    fprintf(stderr, "   --verbose        Verbose logging\n");
    fprintf(stderr, "   --log <level>    Log level for libav backend\n");
    fprintf(stderr, "   --help           Show this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;
    int av_log_level = AV_LOG_QUIET;
    bool verbose = false;
    char *input_file = NULL;
    char *config_file = NULL;

    static struct option long_options[] = {
        {"log", optional_argument, NULL, 'l'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "c:l:hv", long_options, NULL)) != -1) {
        switch (opt) {
        case 'i':
            input_file = optarg;
            break;
        case 'c':
            config_file = optarg;
            break;
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

    if (input_file == NULL || config_file == NULL) {
        MAW_LOG(MAW_ERROR, "Missing required options\n");
        usage();
        return EXIT_FAILURE;
    }

    maw_log_init(verbose, av_log_level);

    (void)maw_yaml_parse(config_file);

    return EXIT_SUCCESS;
}

#else

#include "tests/maw_test.h"

struct Testcase {
    char *desc;
    bool (*fn)(void);
};

struct Testcase testcases[] = {
    { .desc = "Basic test",                 .fn = test_maw_update },
    { .desc = "Dual audio streams",         .fn = test_dual_audio },
    { .desc = "Dual video streams",         .fn = test_dual_video },
};


int main(void) {
    maw_log_init(false, AV_LOG_QUIET);
    int total = sizeof(testcases) / sizeof(struct Testcase);
    int i;
    bool enable_color = (bool)isatty(fileno(stdout));

    fprintf(stdout, "0..%d\n", total - 1);
    for (i = 0; i < total; i++) {
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
            return 1; // XXX
        }
    }

    return 0;
}

#endif


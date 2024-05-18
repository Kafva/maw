#include "maw.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <libavutil/log.h>

#define PROGRAM "maw"

static void usage(void);

static void usage(void) {
    fprintf(stderr, "usage: " PROGRAM " [flags]\n");
    fprintf(stderr, "   --input <file>\n");
    fprintf(stderr, "   --output <file>\n");
    fprintf(stderr, "   --log <level>    Log level for av backend\n");
    fprintf(stderr, "   --help           Show this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;
    int log_level = AV_LOG_QUIET;
    char *input_file = NULL;
    char *config_file = NULL;

    static struct option long_options[] = {
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"log", optional_argument, NULL, 'l'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "i:o:c:l:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'i':
            input_file = optarg;
            break;
        case 'c':
            config_file = optarg;
            break;
        case 'o':
            break;
        case 'l':
            if (strncasecmp("debug", optarg, sizeof("debug") - 1) == 0) {
                log_level = AV_LOG_DEBUG;
            }
            else if (strncasecmp("info", optarg, sizeof("info") - 1) == 0) {
                log_level = AV_LOG_INFO;
            }
            else if (strncasecmp("warning", optarg, sizeof("warning") - 1) == 0) {
                log_level = AV_LOG_WARNING;
            }
            else if (strncasecmp("error", optarg, sizeof("error") - 1) == 0) {
                log_level = AV_LOG_ERROR;
            }
            else if (strncasecmp("quiet", optarg, sizeof("quiet") - 1) == 0) {
                log_level = AV_LOG_QUIET;
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
        MAW_LOG(AV_LOG_ERROR, "Missing required options\n");
        usage();
        return EXIT_FAILURE;
    }

    maw_init(log_level);

    (void)config_file;

    struct Metadata metadata = {
        .title = "New title",
        .album = "New album name",
        .artist = "New artist name",
        .cover_path = "",
        .clear_metadata = true,
    };

    // printf("*** BEFORE\n");
    // (void)maw_dump(input_file);

    if (maw_update(input_file, &metadata) != 0) {
        return EXIT_FAILURE;
    }

    // printf("*** AFTER\n");
    // (void)maw_dump("new.m4a");

    // (void)maw_yaml_parse(config_file);

    return EXIT_SUCCESS;
}

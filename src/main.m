#include "maw.h"

#include <getopt.h>

#define PROGRAM "maw"

// Multi threaded metadata assignment based on yaml config
// '\r' progress printing
// sanitize filenames
// generate playlists
//
//
// make it a macOS only program, use AVAsset api instead of ffmpeg...
// https://wiki.haskell.org/Foreign_Function_Interface
// small objc lib to:
//      Update metadata fields
//      Erase and replace video streams


// Entrypoint outside C to:
//  * We can parse yaml config from C honsetly


static void usage(void);

static void usage(void) {
    fprintf(stderr, "usage: "PROGRAM" [flags]\n");
    fprintf(stderr, "   --input <file>\n");
    fprintf(stderr, "   --output <file>\n");
    fprintf(stderr, "   --help           Show this help message\n");
}

int main(int argc, char *argv[]) {
    int opt;
    char *input_file = NULL;
    char *config_file = NULL;

    static struct option long_options[] = {
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    while ((opt = getopt_long(argc, argv, "i:c:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                input_file = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'h':
                usage();
                return EXIT_FAILURE;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    if (input_file == NULL || config_file == NULL) {
        log(@"Missing required options");
        usage();
        return EXIT_FAILURE;
    }

    (void)config_file;
    const struct Metadata metadata = {
        .artist = @"New red artist",
        .album = @"New red album",
        .cover_path = "music/art/green.png",
        .clear_metadata = false
    };
    printf("*** BEFORE\n");
    (void)maw_dump(input_file);

    (void)maw_update(input_file, &metadata);

    printf("*** AFTER\n");
    (void)maw_dump(input_file);

    // (void)maw_yaml_parse(config_file);

    return EXIT_SUCCESS;
}

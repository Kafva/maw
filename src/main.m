#include "av.h"

#include <getopt.h>

#define PROGRAM "av"

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
    char *output_file = NULL;

    static struct option long_options[] = {
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    while ((opt = getopt_long(argc, argv, "i:o:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                usage();
                return EXIT_FAILURE;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    if (input_file == NULL || output_file == NULL) {
        log(@"Missing required options");
        usage();
        return EXIT_FAILURE;
    }

    (void)av_dump(input_file);
    (void)av_yaml_parse();

    return EXIT_SUCCESS;
}

#import <AVFoundation/AVFoundation.h>
#include <Foundation/Foundation.h>
#include <getopt.h>

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

#define PROGRAM "av"

#define log(FORMAT, ...) \
    fprintf(stderr, "%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])

int av_dump(char*);
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

    return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////

int av_dump(char* filepath) {
    if (access(filepath, F_OK) != 0) {
        log(@"No such file: '%s'", filepath);
        return 1;
    }

    @autoreleasepool {
        NSString *filepath_s = [NSString stringWithUTF8String:filepath];
        NSURL *fileURL = [NSURL fileURLWithPath:filepath_s isDirectory:false];
        AVAsset *asset = [AVAsset assetWithURL: fileURL];

        // Load metadata
        NSArray *metadata = [asset metadata];

        if (metadata.count == 0) {
            log(@"No metadata: '%s'", filepath);
            return 1;
        }
        
        for (AVMetadataItem *item in metadata) {
            log(@"Key: %@, Value: %@", item.commonKey, item.value);
        }
    }

    return 0;
}

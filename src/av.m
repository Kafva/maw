#include <AVFoundation/AVFoundation.h>
#include "av.h"

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



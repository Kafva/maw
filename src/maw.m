#include <AVFoundation/AVFoundation.h>
#include "maw.h"

int maw_dump(const char* filepath) {
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



int maw_update(const char *filepath, const struct Metadata *desired_metadata) {
    if (access(filepath, F_OK) != 0) {
        log(@"No such file: '%s'", filepath);
        return 1;
    }

    (void)desired_metadata;

    @autoreleasepool {
        NSString *filepath_s = [NSString stringWithUTF8String:filepath];
        NSURL *fileURL = [NSURL fileURLWithPath:filepath_s isDirectory:false];
        AVAsset *asset = [AVAsset assetWithURL: fileURL];

        // Create a mutable copy of the metadata
        NSMutableArray *metadata = [asset.metadata mutableCopy];
        //bool titleIsSet = false;
        // int artistIndex = -1;
        // int albumIndex = -1;
        //AVMutableMetadataItem *titleItem = nil;
        // AVMutableMetadataItem *artistItem = nil;
        // AVMutableMetadataItem *albumItem = nil;
        AVMetadataItem *item = nil;
        //AVMutableMetadataItem *mutable_item = nil;

        for (NSUInteger i = 0; i < metadata.count; i++) {
            item = metadata[i];

            if ([item.commonKey isEqualToString:AVMetadataCommonKeyTitle]) {
                if (![[item stringValue] isEqualToString: desired_metadata->title]) {
                    [metadata replaceObjectAtIndex: i
                              withObject: desired_metadata->title];
                }
                //titleIsSet = true;
            }
            // else if ([item.commonKey isEqualToString:AVMetadataCommonKeyArtist] &&
            //     ![[item stringValue] isEqualToString: desired_metadata->artist]) {
            //     artistIndex = i;
            // }
            // else if ([item.commonKey isEqualToString:AVMetadataCommonKeyAlbumName] &&
            //     ![[item stringValue] isEqualToString: desired_metadata->album]) {
            //     albumIndex = i;
            // }
        }

        // Add a new object if the property was missing
        // if (!titleIsSet) {
        //       mutable_item = [AVMutableMetadataItem metadataItem];
        //       mutable_item.key = AVMetadataCommonKeyTitle;
        //       mutable_item. desired_metadata->title;

        //     // item = [AVMetadataItem metadataItemWithValue:desired_metadata->title
        //     //                        identifier:AVMetadataCommonKeyTitle];
        //    [metadata addObject:item];
        // }
        // if (!artistItem) {
        //      artistItem = [AVMutableMetadataItem metadataItem];
        //      artistItem.key = AVMetadataCommonKeyArtist;
        //      [metadata addObject:artistItem];
        // }
        // if (!artistItem) {
        //      artistItem = [AVMutableMetadataItem metadataItem];
        //      artistItem.key = AVMetadataCommonKeyArtist;
        //      [metadata addObject:artistItem];
        // }



        log(@"OK\n");


        // Find the metadata item with the artist name
        // AVMutableMetadataItem *artistItem = nil;
        // for (AVMetadataItem *item in metadata) {
        //     if ([item.commonKey isEqualToString:AVMetadataCommonKeyArtist]) {
        //         artistItem = [item mutableCopy];
        //         break;
        //     }
        //     else if ([item.commonKey isEqualToString:AVMetadataCommonKeyArtist]) {
        // }

        // // If artist item not found, create a new one

        // // Set the new artist name
        // artistItem.value = @"New Artist Name";

        // // Update the metadata of the asset
        // asset.metadata = metadata;

        // // Export the modified asset to a new file
        // AVAssetExportSession *exportSession = [AVAssetExportSession exportSessionWithAsset:asset presetName:AVAssetExportPresetPassthrough];
        // exportSession.outputFileType = AVFileTypeMPEG4;
        // exportSession.outputURL = [NSURL fileURLWithPath:@"path/to/your/output.mp4"];

        // [exportSession exportAsynchronouslyWithCompletionHandler:^{
        //     if (exportSession.status == AVAssetExportSessionStatusCompleted) {
        //         NSLog(@"Export completed successfully");
        //     } else {
        //         NSLog(@"Export failed with error: %@", exportSession.error);
        //     }
        // }];

        // [[NSRunLoop currentRunLoop] run];
    }

    return 0;
}





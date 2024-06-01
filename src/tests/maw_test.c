#include "tests/maw_test.h"

#include "maw.h"

// file with video stream has the video stream stripped
// More than one video/audio stream is rejected
//
// KEEP_CORE_FIELDS    = 0x1,
// KEEP_ALL_FIELDS     = 0x1 << 1,
// KEEP_COVER          = 0x1 << 2,
// CROP_COVER          = 0x1 << 3,

int test_keep_cover(void) {
    int r;
    const char *path = ".testenv/albums/blue/audio_blue_0.m4a";
    const enum MetadataPolicy policy = KEEP_COVER;
    const struct Metadata metadata = {
        .title = "New title",
        .album = "New album name",
        .artist = "New artist name",
        .cover_path = "",
    };

    r = maw_update(path, &metadata, policy);
    if (r != 0) {
        return r;
    }

    // .. Verify ..

    return r;
}

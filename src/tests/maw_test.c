#include "tests/maw_test.h"

#include "maw.h"

// file with video stream has the video stream stripped
// More than one video/audio stream is rejected
//
// KEEP_CORE_FIELDS    = 0x1,
// KEEP_ALL_FIELDS     = 0x1 << 1,
// KEEP_COVER          = 0x1 << 2,
// CROP_COVER          = 0x1 << 3,

int test_maw_update(void) {
    int r;
    const char *path = "./nov.m4a";
    const enum MetadataPolicy policy = KEEP_COVER;
    const struct Metadata metadata = {
        .title = "nov",
        .album = "New album",
        .artist = "New artist",
        .cover_path = "",
    };

    r = maw_update(path, &metadata, policy);
    if (r != 0) {
        return r;
    }

    // No automatic verification 🙈

    return r;
}

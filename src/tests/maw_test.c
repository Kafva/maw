#include <libavutil/error.h>

#include "tests/maw_test.h"
#include "maw.h"

#include <string.h>

// file with video stream has the video stream stripped
// More than one video/audio stream is rejected
//
// KEEP_CORE_FIELDS    = 0x1,
// KEEP_ALL_FIELDS     = 0x1 << 1,
// KEEP_COVER          = 0x1 << 2,
// CROP_COVER          = 0x1 << 3,

bool test_dual_audio(void) {
    int r;
    const char *path = "./.testenv/bad/dual_audio.mp4";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {0};

    r = maw_update(path, &metadata, policy);
    return r == AVERROR_UNKNOWN;
}

bool test_dual_video(void) {
    int r;
    const char *path = "./.testenv/bad/dual_video.mp4";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {0};

    r = maw_update(path, &metadata, policy);
    return r == AVERROR_UNKNOWN;
}

bool test_bad_covers(void) {
    int r;
    const char *path = "./.testenv/albums/blue/audio_blue_0.m4a";
    const enum MetadataPolicy policy = 0;
    const struct Metadata bad_metadata[] = {
        { .cover_path = "./.testenv/bad/dual_audio.mp4" },
        { .cover_path = "./.testenv/bad/only_audio.m4a" },
        { .cover_path = "./does_not_exist" },
        { .cover_path = "./README.md" },
    };

    int errors[] = {
        AVERROR_UNKNOWN,
        AVERROR_UNKNOWN,
        AVERROR(ENOENT),
        AVERROR_INVALIDDATA,
    };

    for (size_t i = 0; i < sizeof(bad_metadata)/sizeof(struct Metadata); i++) {
        r = maw_update(path, &(bad_metadata[i]), policy);
        if (r != errors[i])
            return false;
    }
    
    return true;
}

bool test_keep_cover(void) {
    int r;
    const char *path = "./.testenv/albums/blue/audio_blue_0.m4a";
    const enum MetadataPolicy policy = KEEP_COVER;
    const struct Metadata metadata = {
        .title = "audio_blue_0",
        .album = "Blue album",
        .artist = "Blue artist",
        .cover_path = "",
    };

    r = maw_update(path, &metadata, policy);
    if (r != 0) {
        return r;
    }

    return maw_verify(path, &metadata, policy);
}

bool test_add_cover(void) {
    int r;
    const char *path = "./.testenv/albums/blue/audio_no_cover_blue_0.m4a";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {
        .title = "audio_blue_0",
        .album = "Blue album",
        .artist = "Blue artist",
        .cover_path = "./.testenv/art/blue-1.png",
    };

    r = maw_update(path, &metadata, policy);
    if (r != 0) {
        return r;
    }

    return maw_verify(path, &metadata, policy);
}

bool test_replace_cover(void) {
    int r;
    const char *path = "./.testenv/albums/blue/audio_blue_0.m4a";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {
        .title = "audio_no_blue_0",
        .album = "Blue album",
        .artist = "Blue artist",
        .cover_path = "./.testenv/art/blue-1.png",
    };

    r = maw_update(path, &metadata, policy);
    if (r != 0) {
        return r;
    }

    return maw_verify(path, &metadata, policy);
}

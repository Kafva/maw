#include <libavutil/error.h>

#include "tests/maw_test.h"
#include "maw.h"

#include <string.h>

bool test_dual_audio(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/dual_audio.mp4";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {0};
    (void)desc;

    // Second audio stream should be ignored
    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_no_audio(const char *desc) {
    int r;
    const char *path = "./.testenv/art/blue-1.png";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {0};
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, UNSUPPORTED_INPUT_STREAMS, desc);
    return true;
}

bool test_dual_video(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/dual_video.mp4";
    const enum MetadataPolicy policy = KEEP_COVER;
    const struct Metadata metadata = {
        .title = "dual_video"
    };
    (void)desc;

    // Second video stream should be ignored
    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_bad_covers(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/keep_cover.m4a";
    const enum MetadataPolicy policy = 0;
    const struct Metadata bad_metadata[] = {
        { .cover_path = "./.testenv/unit/dual_audio.mp4" },
        { .cover_path = "./.testenv/unit/only_audio.m4a" },
        { .cover_path = "./does_not_exist" },
        { .cover_path = "./README.md" },
    };
    int errors[] = {
        UNSUPPORTED_INPUT_STREAMS,
        UNSUPPORTED_INPUT_STREAMS,
        AVERROR(ENOENT),
        UNSUPPORTED_INPUT_STREAMS,
    };
    (void)desc;

    for (size_t i = 0; i < sizeof(bad_metadata)/sizeof(struct Metadata); i++) {
        r = maw_update(path, &(bad_metadata[i]), policy);
        MAW_ASSERT_EQ(r, errors[i], bad_metadata->cover_path);
    }

    return true;
}

bool test_crop_cover_policy(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/crop_cover.m4a";
    const enum MetadataPolicy policy = CROP_COVER;
    const struct Metadata metadata = {0};
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return false; // TODO
}

bool test_keep_all_fields_policy(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/keep_all_fields.m4a";
    const enum MetadataPolicy policy = KEEP_ALL_FIELDS;
    const struct Metadata metadata = {
        .title = "keep_all_fields",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
    };
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return false; // TODO
}

bool test_keep_core_fields_policy(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/keep_core_fields.m4a";
    const enum MetadataPolicy policy = KEEP_CORE_FIELDS;
    const struct Metadata metadata = {
        .title = "keep_core_fields",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
    };
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_keep_cover_policy(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/keep_cover.m4a";
    const enum MetadataPolicy policy = KEEP_COVER;
    const struct Metadata metadata = {0};
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_add_cover(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/add_cover.m4a";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {
        .title = "add_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
    };
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_replace_cover(const char *desc) {
    int r;
    const char *path = "./.testenv/unit/replace_cover.m4a";
    const enum MetadataPolicy policy = 0;
    const struct Metadata metadata = {
        .title = "replace_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
    };
    (void)desc;

    r = maw_update(path, &metadata, policy);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(path, &metadata, policy);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

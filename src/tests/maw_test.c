#include <libavutil/error.h>

#include "tests/maw_test.h"
#include "tests/maw_verify.h"
#include "maw.h"

#include <string.h>

bool test_dual_audio(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath =  "./.testenv/unit/dual_audio.mp4"
    };
    (void)desc;

    // Second audio stream should be ignored
    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_no_audio(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath = "./.testenv/art/blue-1.png"
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, UNSUPPORTED_INPUT_STREAMS, desc);
    return true;
}

bool test_dual_video(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath = "./.testenv/unit/dual_video.mp4",
        .title = "dual_video"
    };
    (void)desc;

    // Second video stream should be ignored
    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Metadata ////////////////////////////////////////////////////////////////////

bool test_keep_all(const char *desc) {
    int r;
    // Default policy: keep everything (except explicitly set metadata fields) as is
    const Metadata metadata = {
        .filepath = "./.testenv/unit/keep_all.m4a",
        .title = "keep_all",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        // .cover_policy = KEEP_COVER_UNLESS_PROVIDED // (implicit)
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_clear_non_core_fields(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath =  "./.testenv/unit/clear_non_core_fields.m4a",
        .title = "clear_non_core_fields",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        .clear_non_core_fields = true,
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Covers //////////////////////////////////////////////////////////////////////

bool test_bad_covers(const char *desc) {
    int r;
    const Metadata bad_metadata[] = {
        { .filepath = "./.testenv/unit/keep_all.m4a", .cover_path = "./.testenv/unit/dual_audio.mp4" },
        { .filepath = "./.testenv/unit/keep_all.m4a", .cover_path = "./.testenv/unit/only_audio.m4a" },
        { .filepath = "./.testenv/unit/keep_all.m4a", .cover_path = "./does_not_exist" },
        { .filepath = "./.testenv/unit/keep_all.m4a", .cover_path = "./README.md" },
    };
    int errors[] = {
        UNSUPPORTED_INPUT_STREAMS,
        UNSUPPORTED_INPUT_STREAMS,
        AVERROR(ENOENT),
        AVERROR_INVALIDDATA,
        // UNSUPPORTED_INPUT_STREAMS,
    };
    (void)desc;

    for (size_t i = 0; i < sizeof(bad_metadata)/sizeof(Metadata); i++) {
        r = maw_update(&(bad_metadata[i]));
        MAW_ASSERT_EQ(r, errors[i], bad_metadata[i].cover_path);
    }

    return true;
}

bool test_crop_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath =  "./.testenv/unit/crop_cover.m4a",
        .cover_policy = CROP_COVER,
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);

    // Verify that a second update is idempotent
    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);

    return true;
}

bool test_clear_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath = "./.testenv/unit/clear_cover.m4a",
        .cover_policy = CLEAR_COVER
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_add_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath = "./.testenv/unit/add_cover.m4a",
        .title = "add_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_replace_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .filepath = "./.testenv/unit/replace_cover.m4a",
        .title = "replace_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
    };
    (void)desc;

    r = maw_update(&metadata);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&metadata);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Jobs ////////////////////////////////////////////////////////////////////////

bool test_jobs(const char *desc) {
    return false;
}

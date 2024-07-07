#include "maw/job.h"
#include "maw/cfg.h"
#include "maw/tests/maw_test.h"
#include "maw/tests/maw_verify.h"
#include "maw/maw.h"

#include <libavutil/error.h>
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
        // .cover_policy = COVER_KEEP // (implicit)
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
        .filepath =  "./.testenv/unit/clean.m4a",
        .title = "clean",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        .clean = true,
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
        .cover_policy = COVER_CROP,
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
        .cover_policy = COVER_CLEAR
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

bool test_job_ok(const char *desc) {
    int r;
    Metadata arr[] = {
        {
            .filepath = ".testenv/albums/blue/audio_blue_0.m4a",
            .title = "audio_blue_0",
            .album = "New blue",
            .cover_path = "./.testenv/art/blue-1.png",
        },
        {
            .filepath = ".testenv/albums/red/audio_red_0.m4a",
            .title = "audio_red_0",
            .album = "New red"
        },
        {
            .filepath = ".testenv/albums/red/audio_red_1.m4a",
            .title = "audio_red_1",
            .album = "New red"
        },
        {
            .filepath = ".testenv/albums/red/audio_red_2.m4a",
            .title = "audio_red_2",
            .album = "New red"
        },
    };
    size_t arrsize = sizeof(arr) / sizeof(Metadata);

    r = maw_job_launch(arr, arrsize, 3);
    MAW_ASSERT_EQ(r, 0, desc);

    for (size_t i = 0; i < arrsize; i++) {
        r = maw_verify(&arr[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    return true;
}

bool test_job_error(const char *desc) {
    int r;
    Metadata arr[] = {
        {
            .filepath = ".testenv/albums/blue/audio_blue_0.m4a",
            .title = "audio_blue_0",
            .album = "New blue",
            .cover_path = "./.testenv/art/blue-1.png",
        },
        {
            // BAD
            .filepath = "non_existant",
        },
        {
            .filepath = ".testenv/albums/red/audio_red_1.m4a",
            .title = "audio_red_1",
            .album = "New red"
        },
        {
            .filepath = ".testenv/albums/red/audio_red_2.m4a",
            .title = "audio_red_2",
            .album = "New red"
        },
    };
    size_t arrsize = sizeof(arr) / sizeof(Metadata);

    r = maw_job_launch(arr, arrsize, 2);
    MAW_ASSERT_EQ(r, -1, desc);

    return true;
}

// Configuration ///////////////////////////////////////////////////////////////

bool test_cfg_ok(const char *desc) {
    int r;
    const char *config_file = ".testenv/maw.yml";
    MawConfig *cfg = NULL;

    r = maw_cfg_parse(config_file, &cfg);
    MAW_ASSERT_EQ(r, 0, desc);
    maw_cfg_free(cfg);

    return true;
}

bool test_cfg_error(const char *desc) {
    int r;
    const char *config_file = ".testenv/unit/bad.yml";
    MawConfig *cfg = NULL;

    r = maw_cfg_parse(config_file, &cfg);
    MAW_ASSERT_EQ(r, INTERNAL_ERROR, desc);
    maw_cfg_free(cfg);

    return true;
}


#include "maw/tests/maw_test.h"
#include "maw/cfg.h"
#include "maw/job.h"
#include "maw/maw.h"
#include "maw/tests/maw_verify.h"
#include "maw/utils.h"

#include <libavutil/error.h>
#include <string.h>
static const Metadata no_metadata = {0};

bool test_dual_audio(const char *desc) {
    int r;
    const MediaFile mediafile = {.path = "./.testenv/unit/dual_audio.mp4",
                                 .metadata = &no_metadata};
    (void)desc;

    // Second audio stream should be ignored
    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_no_audio(const char *desc) {
    int r;
    const MediaFile mediafile = {.path = "./.testenv/art/blue-1.png",
                                 .metadata = &no_metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, MAW_ERR_UNSUPPORTED_INPUT_STREAMS, desc);
    return true;
}

bool test_dual_video(const char *desc) {
    int r;
    const Metadata metadata = {.title = "dual_video"};
    const MediaFile mediafile = {.path = "./.testenv/unit/dual_video.mp4",
                                 .metadata = &metadata};
    (void)desc;

    // Second video stream should be ignored
    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Metadata ////////////////////////////////////////////////////////////////////

bool test_keep_all(const char *desc) {
    int r;
    // Default policy: keep everything (except explicitly set mediafile fields)
    // as is
    const Metadata metadata = {
        .title = "keep_all",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        // .cover_policy = COVER_UNSPECIFIED // (implicit)
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/keep_all.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_clear_non_core_fields(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "clean",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        .clean = true,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/clean.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Covers //////////////////////////////////////////////////////////////////////

bool test_bad_covers(const char *desc) {
    int r;
    const Metadata bad_metadata[] = {
        {.cover_path = "./.testenv/unit/dual_audio.mp4"},
        {.cover_path = "./.testenv/unit/only_audio.m4a"},
        {.cover_path = "./does_not_exist"},
        {.cover_path = "./README.md"},
    };
    MediaFile mediafile = {.path = "./.testenv/unit/keep_all.m4a",
                           .metadata = NULL};
    int errors[] = {
        MAW_ERR_UNSUPPORTED_INPUT_STREAMS, MAW_ERR_UNSUPPORTED_INPUT_STREAMS,
        AVERROR(ENOENT), AVERROR_INVALIDDATA,
        // UNSUPPORTED_INPUT_STREAMS,
    };
    (void)desc;

    for (size_t i = 0; i < sizeof(bad_metadata) / sizeof(Metadata); i++) {
        mediafile.metadata = &bad_metadata[i];
        r = maw_update(&mediafile);
        MAW_ASSERT_EQ(r, errors[i], bad_metadata[i].cover_path);
    }

    return true;
}

bool test_crop_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .cover_policy = COVER_CROP,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/crop_cover.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);

    // Verify that a second update is idempotent
    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);

    return true;
}

bool test_clear_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .cover_policy = COVER_CLEAR,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/clear_cover.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_add_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "add_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/add_cover.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

bool test_replace_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "replace_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/replace_cover.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, 0, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Jobs ////////////////////////////////////////////////////////////////////////

bool test_job_ok(const char *desc) {
    int r;
    Metadata cfg_arr[] = {
        {
            .title = "audio_blue_0",
            .album = "New blue",
            .cover_path = "./.testenv/art/blue-1.png",
        },
        {.title = "audio_red_0", .album = "New red"},
        {.title = "audio_red_1", .album = "New red"},
        {.title = "audio_red_2", .album = "New red"},
    };
    MediaFile mediafiles[] = {
        {.path = ".testenv/albums/blue/audio_blue_0.m4a",
         .metadata = &cfg_arr[0]},
        {.path = ".testenv/albums/red/audio_red_0.m4a",
         .metadata = &cfg_arr[1]},
        {.path = ".testenv/albums/red/audio_red_1.m4a",
         .metadata = &cfg_arr[2]},
        {.path = ".testenv/albums/red/audio_red_2.m4a",
         .metadata = &cfg_arr[3]},
    };

    ssize_t mediafiles_count = sizeof(mediafiles) / sizeof(MediaFile);

    r = maw_job_launch(mediafiles, mediafiles_count, 3);
    MAW_ASSERT_EQ(r, 0, desc);

    for (ssize_t i = 0; i < mediafiles_count; i++) {
        r = maw_verify(&mediafiles[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    return true;
}

bool test_job_error(const char *desc) {
    int r;
    Metadata cfg_arr[] = {
        {
            .title = "audio_blue_0",
            .album = "New blue",
            .cover_path = "./.testenv/art/blue-1.png",
        },
        {.title = "audio_red_0", .album = "New red"},
        {.title = "audio_red_1", .album = "New red"},
        {.title = "audio_red_2", .album = "New red"},
    };
    MediaFile mediafiles[] = {
        {
            .path = ".testenv/albums/blue/audio_blue_0.m4a",
            .metadata = &cfg_arr[0],
        },
        {// BAD
         .path = "non_existant",
         .metadata = &cfg_arr[1]},
        {.path = ".testenv/albums/red/audio_red_1.m4a",
         .metadata = &cfg_arr[2]},
        {.path = ".testenv/albums/red/audio_red_2.m4a",
         .metadata = &cfg_arr[3]},
    };
    ssize_t mediafiles_count = sizeof(cfg_arr) / sizeof(Metadata);

    r = maw_job_launch(mediafiles, mediafiles_count, 2);
    MAW_ASSERT_EQ(r, -1, desc);

    return true;
}

// Configuration ///////////////////////////////////////////////////////////////

bool test_cfg_ok(const char *desc) {
    int r;
    const char *config_file = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    ssize_t mediafiles_count = 0;

    r = maw_cfg_parse(config_file, &cfg);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_cfg_alloc_mediafiles(cfg, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, 0, desc);

    maw_cfg_dump(cfg);

    maw_cfg_free(cfg);
    maw_mediafiles_free(mediafiles, mediafiles_count);

    return true;
}

bool test_cfg_error(const char *desc) {
    int r;
    const char *config_file = ".testenv/unit/bad.yml";
    MawConfig *cfg = NULL;

    r = maw_cfg_parse(config_file, &cfg);
    MAW_ASSERT_EQ(r, MAW_ERR_YAML, desc);
    maw_cfg_free(cfg);

    return true;
}

bool test_hash(const char *desc) {
    uint32_t digest;
    const char *data = "ABC";
    digest = hash(data);

    // Reference value from: go/src/hash/fnv/fnv.go
    MAW_ASSERT_EQ(digest, 1552166763, desc);

    return true;
}

bool test_complete(const char *desc) {
    int r;
    const char *config_file = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    ssize_t mediafiles_count = 0;

    r = maw_cfg_parse(config_file, &cfg);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_cfg_alloc_mediafiles(cfg, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_job_launch(mediafiles, mediafiles_count, 3);
    MAW_ASSERT_EQ(r, 0, desc);

    for (ssize_t i = 0; i < mediafiles_count; i++) {
        r = maw_verify(&mediafiles[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    maw_cfg_free(cfg);
    maw_mediafiles_free(mediafiles, mediafiles_count);

    return true;
}

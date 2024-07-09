#include "maw/tests/maw_test.h"
#include "maw/cfg.h"
#include "maw/mediafiles.h"
#include "maw/playlists.h"
#include "maw/tests/maw_verify.h"
#include "maw/threads.h"
#include "maw/update.h"
#include "maw/utils.h"

#include <libavutil/error.h>
#include <string.h>

static const Metadata no_metadata = {0};

static bool test_dual_audio(const char *desc) {
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

static bool test_no_audio(const char *desc) {
    int r;
    const MediaFile mediafile = {.path = "./.testenv/art/blue-1.png",
                                 .metadata = &no_metadata};
    (void)desc;

    r = maw_update(&mediafile);
    MAW_ASSERT_EQ(r, MAW_ERR_UNSUPPORTED_INPUT_STREAMS, desc);
    return true;
}

static bool test_dual_video(const char *desc) {
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

static bool test_keep_all(const char *desc) {
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

static bool test_clear_non_core_fields(const char *desc) {
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

static bool test_bad_covers(const char *desc) {
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

static bool test_crop_cover(const char *desc) {
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

static bool test_clear_cover(const char *desc) {
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

static bool test_add_cover(const char *desc) {
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

static bool test_replace_cover(const char *desc) {
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

static bool test_threads_ok(const char *desc) {
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

    r = maw_threads_launch(mediafiles, mediafiles_count, 3);
    MAW_ASSERT_EQ(r, 0, desc);

    for (ssize_t i = 0; i < mediafiles_count; i++) {
        r = maw_verify(&mediafiles[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    return true;
}

static bool test_threads_error(const char *desc) {
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

    r = maw_threads_launch(mediafiles, mediafiles_count, 2);
    MAW_ASSERT_EQ(r, -1, desc);

    return true;
}

static bool test_update(const char *desc) {
    int r;
    const char *config_path = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    ssize_t mediafiles_count = 0;
    char *folders[] = {"red"};
    size_t music_dir_pathlen;
    MawArguments args = {
        .cmd_args = folders,
        .cmd_args_count = 1,
        .thread_count = 3,
    };

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_mediafiles_alloc(cfg, &args, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, 0, desc);

    // Only paths starting with 'red' should have been included
    music_dir_pathlen = strlen(cfg->music_dir) + 1;
    for (int i = 0; i < mediafiles_count; i++) {
        r = STR_HAS_PREFIX(mediafiles[i].path + music_dir_pathlen, "red");
        MAW_ASSERT_EQ(r, true, desc);
    }

    r = maw_threads_launch(mediafiles, mediafiles_count, args.thread_count);
    MAW_ASSERT_EQ(r, 0, desc);

    for (ssize_t i = 0; i < mediafiles_count; i++) {
        r = maw_verify(&mediafiles[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    maw_cfg_free(cfg);
    maw_mediafiles_free(mediafiles, mediafiles_count);

    return true;
}

static bool test_playlists(const char *desc) {
    int r;
    const char *config_path = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    const char *playlist = ".testenv/albums/.second.m3u";
    const char *expected = "blue/audio_blue_1.m4a\n"
                           "blue/audio_blue_2.m4a\n"
                           "red/audio_red_0.m4a\n"
                           "red/audio_red_1.m4a\n"
                           "red/audio_red_2.m4a\n"
                           "red/audio_red_3.m4a\n";

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_playlists_gen(cfg);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_verify_file(playlist, expected);
    MAW_ASSERT_EQ(r, true, desc);

    maw_cfg_free(cfg);

    return true;
}

// Configuration ///////////////////////////////////////////////////////////////

static bool test_cfg_ok(const char *desc) {
    int r;
    const char *config_path = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    ssize_t mediafiles_count = 0;
    MawArguments args = {0};

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, 0, desc);

    r = maw_mediafiles_alloc(cfg, &args, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, 0, desc);

    maw_cfg_dump(cfg);

    maw_cfg_free(cfg);
    maw_mediafiles_free(mediafiles, mediafiles_count);

    return true;
}

static bool test_cfg_error(const char *desc) {
    int r;
    const char *config_path = ".testenv/unit/bad.yml";
    MawConfig *cfg = NULL;

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, MAW_ERR_YAML, desc);
    maw_cfg_free(cfg);

    return true;
}

static bool test_hash(const char *desc) {
    uint32_t digest;
    const char *data = "ABC";
    digest = hash(data);

    // Reference value from: go/src/hash/fnv/fnv.go
    MAW_ASSERT_EQ(digest, 1552166763, desc);

    return true;
}

// Runner //////////////////////////////////////////////////////////////////////

static struct Testcase testcases[] = {
    {.desc = "Keep metadata and cover", .fn = test_keep_all},
    {.desc = "Clear non core fields", .fn = test_clear_non_core_fields},
    {.desc = "Clear cover", .fn = test_clear_cover},
    {.desc = "Add cover", .fn = test_add_cover},
    {.desc = "Replace cover", .fn = test_replace_cover},
    {.desc = "Bad covers", .fn = test_bad_covers},
    {.desc = "No audio streams", .fn = test_no_audio},
    {.desc = "Dual audio streams", .fn = test_dual_audio},
    {.desc = "Dual video streams", .fn = test_dual_video},
    {.desc = "Crop cover", .fn = test_crop_cover},
    {.desc = "Jobs ok", .fn = test_threads_ok},
    {.desc = "Jobs error", .fn = test_threads_error},
    {.desc = "Configuration ok", .fn = test_cfg_ok},
    {.desc = "Configuration error", .fn = test_cfg_error},
    {.desc = "FNV-1a Hash", .fn = test_hash},
    {.desc = "Update command", .fn = test_update},
    {.desc = "Playlists command", .fn = test_playlists}};

int run_tests(const char *match_testcase) {
    int total = sizeof(testcases) / sizeof(struct Testcase);
    int i;
    int r;
    bool enable_color = isatty(fileno(stdout)) && isatty(fileno(stderr));
    FILE *tfd = stdout;

    fprintf(tfd, "0..%d\n", total - 1);
    for (i = 0; i < total; i++) {
        if (match_testcase != NULL) {
            r = strncasecmp(match_testcase, testcases[i].desc,
                            strlen(match_testcase));
            if (r != 0) {
                if (enable_color)
                    fprintf(tfd, "\033[38;5;246mok\033[0m %d - %s # skip\n", i,
                            testcases[i].desc);
                else
                    fprintf(tfd, "ok %d - %s # skip\n", i, testcases[i].desc);
                continue;
            }
        }

        if (testcases[i].fn(testcases[i].desc)) {
            if (enable_color)
                fprintf(tfd, "\033[92mok\033[0m %d - %s\n", i,
                        testcases[i].desc);
            else
                fprintf(tfd, "ok %d - %s\n", i, testcases[i].desc);
        }
        else {
            if (enable_color)
                fprintf(tfd, "\033[91mnot ok\033[0m %d - %s\n", i,
                        testcases[i].desc);
            else
                fprintf(tfd, "not ok %d - %s\n", i, testcases[i].desc);
            return EXIT_FAILURE; // XXX
        }
    }

    return EXIT_SUCCESS;
}

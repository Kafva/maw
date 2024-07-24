#include "maw/tests/maw_test.h"
#include "maw/cfg.h"
#include "maw/maw.h"
#include "maw/playlists.h"
#include "maw/tests/maw_verify.h"
#include "maw/threads.h"
#include "maw/update.h"
#include "maw/utils.h"

#include <libavutil/error.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Perform an update on the provided media file twice after waiting for a short
// period. Verify that the modification time after the second update has not
// changed.
#define NOOP_CHECK(m) \
    do { \
        struct stat s_first; \
        struct stat s_second; \
        r = maw_update(&m, false); \
        MAW_ASSERT_EQ(r, RESULT_OK, desc); \
\
        r = maw_verify(&m); \
        MAW_ASSERT_EQ(r, true, desc); \
\
        if (stat(m.path, &s_first) != 0) { \
            MAW_PERRORF("stat", m.path); \
            return false; \
        } \
\
        usleep(500000); \
\
        r = maw_update(&m, false); \
        MAW_ASSERT_EQ(r, RESULT_NOOP, desc); \
\
        r = maw_verify(&m); \
        MAW_ASSERT_EQ(r, true, desc); \
\
        if (stat(m.path, &s_second) != 0) { \
            MAW_PERRORF("stat", m.path); \
            return false; \
        } \
\
        MAW_ASSERT_EQ((int)s_second.st_mtime, (int)s_first.st_mtime, \
                      "Modification time has changed"); \
    } while (0)

static const Metadata no_metadata = {0};

static bool test_dual_audio(const char *desc) {
    int r;
    const Metadata metadata = {.clean_policy = CLEAN_POLICY_TRUE};
    const MediaFile mediafile = {.path = "./.testenv/unit/dual_audio.mp4",
                                 .metadata = &metadata};
    (void)desc;

    // Second audio stream should be ignored
    r = maw_update(&mediafile, false);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

static bool test_no_audio(const char *desc) {
    int r;
    const MediaFile mediafile = {.path = "./.testenv/art/blue-1.png",
                                 .metadata = &no_metadata};
    (void)desc;

    r = maw_update(&mediafile, false);
    MAW_ASSERT_EQ(r, RESULT_NOOP,
                  desc); // OK return value, non mp4 files are skipped
    return true;
}

static bool test_dual_video(const char *desc) {
    int r;
    const Metadata metadata = {.title = "dual_video",
                               .clean_policy = CLEAN_POLICY_TRUE};
    const MediaFile mediafile = {.path = "./.testenv/unit/dual_video.mp4",
                                 .metadata = &metadata};
    (void)desc;

    // Second video stream should be ignored
    r = maw_update(&mediafile, false);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// Metadata ////////////////////////////////////////////////////////////////////

static bool test_keep_all(const char *desc) {
    int r;
    // Keep everything (except explicitly set mediafile fields)
    // as is. We only verify that one of the non-core fields are actually kept.
    const Metadata metadata = {
        .title = "Keep all",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        // .cover_policy = COVER_POLICY_KEEP // (implicit)
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/keep_all.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile, false);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

static bool test_auto_title(const char *desc) {
    int r;
    // Title should be automatically set to the filename by default
    Metadata metadata = {0};
    const MediaFile mediafile = {.path = "./.testenv/unit/auto_set_title.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile, false);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    metadata.title = "auto_set_title";
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);

    return true;
}

static bool test_clear_non_core_fields(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "Clean name",
        .album = "New album",
        .artist = "New artist",
        .cover_path = NULL,
        .cover_policy = COVER_POLICY_UNSPECIFIED,
        .clean_policy = CLEAN_POLICY_TRUE,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/clean.m4a",
                                 .metadata = &metadata};
    (void)desc;

    r = maw_update(&mediafile, false);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);
    r = maw_verify(&mediafile);
    MAW_ASSERT_EQ(r, true, desc);
    return true;
}

// NOOP ////////////////////////////////////////////////////////////////////////

// The NOOP tests also cover most of the cover art functionality

static bool test_noop(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "Keep modification time",
        .album = "New album",
        .artist = "New artist",
        .cover_policy = COVER_POLICY_UNSPECIFIED,
        .clean_policy = CLEAN_POLICY_UNSPECIFIED,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/noop.m4a",
                                 .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);

    return true;
}

static bool test_noop_clean(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "Keep modification time",
        .album = "New album",
        .artist = "New artist",
        .cover_policy = COVER_POLICY_UNSPECIFIED,
        .clean_policy = CLEAN_POLICY_TRUE,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/noop_clean.m4a",
                                 .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);

    return true;
}

static bool test_noop_add_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "add_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
        .cover_policy = COVER_POLICY_PATH,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/noop_add_cover.m4a",
                                 .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);
    return true;
}

static bool test_noop_replace_cover(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "replace_cover",
        .album = NULL,
        .artist = NULL,
        .cover_path = "./.testenv/art/blue-1.png",
        .cover_policy = COVER_POLICY_PATH,
    };
    const MediaFile mediafile = {.path =
                                     "./.testenv/unit/noop_replace_cover.m4a",
                                 .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);
    return true;
}

static bool test_noop_cover_crop(const char *desc) {
    int r;
    const Metadata metadata = {
        .cover_policy = COVER_POLICY_CROP,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/noop_cover_crop.m4a",
                                 .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);

    return true;
}

static bool test_noop_nocover_crop(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "Keep modification time",
        .cover_policy = COVER_POLICY_CROP,
    };
    const MediaFile mediafile = {
        .path = "./.testenv/unit/noop_nocover_crop.m4a", .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);

    return true;
}

static bool test_noop_cover_clear(const char *desc) {
    int r;
    const Metadata metadata = {
        .title = "Keep modification time",
        .cover_policy = COVER_POLICY_CLEAR,
    };
    const MediaFile mediafile = {.path = "./.testenv/unit/noop_cover_clear.m4a",
                                 .metadata = &metadata};
    (void)desc;

    NOOP_CHECK(mediafile);

    return true;
}

// Covers //////////////////////////////////////////////////////////////////////

static bool test_bad_covers(const char *desc) {
    int r;
    // clang-format off
    const Metadata bad_metadata[] = {
        {.cover_policy = COVER_POLICY_PATH, .cover_path = "./.testenv/unit/dual_audio.mp4"},
        {.cover_policy = COVER_POLICY_PATH, .cover_path = "./.testenv/unit/only_audio.m4a"},
        {.cover_policy = COVER_POLICY_PATH, .cover_path = "./does_not_exist"},
        {.cover_policy = COVER_POLICY_PATH, .cover_path = "./README.md"},
        {.cover_policy = COVER_POLICY_CROP, .cover_path = "./.testenv/art/blue-1.png"},
    };
    int errors[] = {
        RESULT_UNSUPPORTED_INPUT_STREAMS,
        RESULT_UNSUPPORTED_INPUT_STREAMS,
        AVERROR(ENOENT),
        AVERROR_INVALIDDATA,
        RESULT_ERR_INTERNAL
    };
    // clang-format on
    MediaFile mediafile = {.path = "./.testenv/unit/keep_all.m4a",
                           .metadata = NULL};
    (void)desc;

    for (size_t i = 0; i < sizeof(bad_metadata) / sizeof(Metadata); i++) {
        mediafile.metadata = &bad_metadata[i];
        r = maw_update(&mediafile, false);
        MAW_ASSERT_EQ(r, errors[i], bad_metadata[i].cover_path);
    }

    return true;
}

// Threads /////////////////////////////////////////////////////////////////////

static bool test_threads_ok(const char *desc) {
    int r;
    Metadata cfg_arr[] = {
        {
            .title = "audio_blue_0",
            .album = "New blue",
            .cover_path = "./.testenv/art/blue-1.png",
            .cover_policy = COVER_POLICY_PATH,
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

    size_t mediafiles_count = sizeof(mediafiles) / sizeof(MediaFile);

    r = maw_threads_launch(mediafiles, mediafiles_count, 3, false);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    for (size_t i = 0; i < mediafiles_count; i++) {
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
            .cover_policy = COVER_POLICY_PATH,
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
        {
            .path = ".testenv/albums/blue/audio_blue_1.m4a",
            .metadata = NULL,
        }, // Should result in failure
        {.path = ".testenv/albums/red/audio_red_2.m4a",
         .metadata = &cfg_arr[2]},
        {.path = ".testenv/albums/red/audio_red_3.m4a",
         .metadata = &cfg_arr[3]},
    };
    size_t mediafiles_count = sizeof(cfg_arr) / sizeof(Metadata);

    r = maw_threads_launch(mediafiles, mediafiles_count, 2, false);
    MAW_ASSERT_EQ(r, -1, desc);

    return true;
}

static bool test_update(const char *desc) {
    int r;
    const char *config_path = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    size_t mediafiles_count = 0;
    char *folders[] = {"red"};
    size_t music_dir_pathlen;
    MawArguments args = {.cmd_args = folders,
                         .cmd_args_count = 1,
                         .thread_count = 2,
                         .dry_run = false};

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    r = maw_update_load(cfg, &args, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    // Only paths starting with 'red' should have been included
    music_dir_pathlen = strlen(cfg->music_dir) + 1;
    for (size_t i = 0; i < mediafiles_count; i++) {
        r = STR_HAS_PREFIX(mediafiles[i].path + music_dir_pathlen, "red");
        MAW_ASSERT_EQ(r, true, desc);
    }

    r = maw_threads_launch(mediafiles, mediafiles_count, args.thread_count,
                           args.dry_run);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    for (size_t i = 0; i < mediafiles_count; i++) {
        r = maw_verify(&mediafiles[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    maw_cfg_free(cfg);
    maw_update_free(mediafiles, mediafiles_count);

    return true;
}

static bool test_update_override(const char *desc) {
    int r;
    const char *config_path = ".testenv/maw.yml";
    MawConfig *cfg = NULL;
    MediaFile mediafiles[MAW_MAX_FILES];
    size_t mediafiles_count = 0;
    size_t music_dir_pathlen;
    MawArguments args = {.cmd_args = NULL,
                         .cmd_args_count = 0,
                         .thread_count = 1,
                         .dry_run = false};

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    r = maw_update_load(cfg, &args, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    // Only paths starting with 'blue' should have been included
    music_dir_pathlen = strlen(cfg->music_dir) + 1;
    for (size_t i = 0; i < mediafiles_count; i++) {
        // The blue/audio_blue_2.m4a entry specifies the 'NONE' keyword for
        // 'cover', this should result in the original cover being kept.
        if (STR_EQ(mediafiles[i].path + music_dir_pathlen,
                   "blue/audio_blue_2.m4a")) {
            r = mediafiles[i].metadata->cover_policy == COVER_POLICY_KEEP;
            MAW_ASSERT_EQ(r, true, desc);
        }
    }

    r = maw_threads_launch(mediafiles, mediafiles_count, args.thread_count,
                           args.dry_run);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    for (size_t i = 0; i < mediafiles_count; i++) {
        r = maw_verify(&mediafiles[i]);
        MAW_ASSERT_EQ(r, true, desc);
    }

    maw_cfg_free(cfg);
    maw_update_free(mediafiles, mediafiles_count);

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
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    r = maw_playlists_gen(cfg);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

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
    size_t mediafiles_count = 0;
    MawArguments args = {0};

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    r = maw_update_load(cfg, &args, mediafiles, &mediafiles_count);
    MAW_ASSERT_EQ(r, RESULT_OK, desc);

    maw_cfg_free(cfg);
    maw_update_free(mediafiles, mediafiles_count);

    return true;
}

static bool test_cfg_key_missing_value(const char *desc) {
    int r;
    const char *config_path = ".testenv/unit/key_missing_value.yml";
    MawConfig *cfg = NULL;

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, RESULT_ERR_INTERNAL, desc);

    maw_cfg_free(cfg);

    return true;
}

static bool test_cfg_error(const char *desc) {
    int r;
    const char *config_path = ".testenv/unit/bad.yml";
    MawConfig *cfg = NULL;

    r = maw_cfg_parse(config_path, &cfg);
    MAW_ASSERT_EQ(r, RESULT_ERR_YAML, desc);
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

// clang-format off
static struct Testcase testcases[] = {
    {.desc = "Keep metadata and cover", .fn = test_keep_all},
    {.desc = "Autoset title", .fn = test_auto_title},
    {.desc = "Clear non core fields", .fn = test_clear_non_core_fields},
    {.desc = "Bad covers", .fn = test_bad_covers},
    {.desc = "No audio streams", .fn = test_no_audio},
    {.desc = "Dual audio streams", .fn = test_dual_audio},
    {.desc = "Dual video streams", .fn = test_dual_video},
    {.desc = "Threads ok", .fn = test_threads_ok},
    {.desc = "Threads error", .fn = test_threads_error},
    {.desc = "YAML ok", .fn = test_cfg_ok},
    {.desc = "YAML key missing value", .fn = test_cfg_key_missing_value},
    {.desc = "YAML invalid", .fn = test_cfg_error},
    {.desc = "FNV-1a Hash", .fn = test_hash},
    {.desc = "Update command", .fn = test_update},
    {.desc = "Update override cover", .fn = test_update_override},
    {.desc = "Playlists command", .fn = test_playlists},
    {.desc = "NOOP metadata", .fn = test_noop},
    {.desc = "NOOP metadata clean", .fn = test_noop_clean},
    {.desc = "NOOP Add cover", .fn = test_noop_add_cover},
    {.desc = "NOOP Replace cover", .fn = test_noop_replace_cover},
    {.desc = "NOOP Crop cover", .fn = test_noop_cover_crop},
    {.desc = "NOOP Crop no cover on source", .fn = test_noop_nocover_crop},
    {.desc = "NOOP cover clear configuration", .fn = test_noop_cover_clear},
};
// clang-format on

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

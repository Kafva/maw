#ifndef MAW_H
#define MAW_H

#include <errno.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/types.h>
#ifdef __linux__
#include <stdint.h>
#endif

// TAILQ is used instead of STAILQ to make the code more portable.
// STAILQ_LAST is only provided by <bsd/sys/queue.h> and generates
// -Wgnu-statement-expression-from-macro-expansion warnings on Linux,
// TAILQ_LAST works fine on both Linux and BSD.

// Maximum number of files to handle in one invocation
#define MAW_MAX_FILES 1024

// Maximum length of paths in configuration file etc.
#define MAW_PATH_MAX 1024

// The cover policy options are mutually exclusive from one another
enum CoverPolicy {
    // Keep original cover art unless a custom `cover_path` is given (default)
    COVER_UNSPECIFIED = 0,
    // Explicitly keep the current cover as is
    COVER_KEEP = 1,
    // Remove cover art if present
    COVER_CLEAR = 2,
    // Crop 1280x720 covers to 720x720, idempotent for 720x720 covers.
    COVER_CROP = 3,
} typedef CoverPolicy;

enum MawError {
    // Fallback error code for maw functions
    MAW_ERR_INTERNAL = 50,
    // Input file has an unsupported set of streams
    MAW_ERR_UNSUPPORTED_INPUT_STREAMS = 51,
    // Error encountered in libyaml
    MAW_ERR_YAML = 52,
};

struct Metadata {
    const char *title;
    const char *album;
    const char *artist;
    const char *cover_path;
    CoverPolicy cover_policy;
    bool clean;
} typedef Metadata;

struct MediaFile {
    const char *path;
    const Metadata *metadata;
    uint32_t path_digest;
} typedef MediaFile;

struct PlaylistPath {
    const char *path;
    TAILQ_ENTRY(PlaylistPath) entry;
} typedef PlaylistPath;

struct Playlist {
    const char *name;
    TAILQ_HEAD(, PlaylistPath) playlist_paths_head;
} typedef Playlist;

struct PlaylistEntry {
    Playlist value;
    TAILQ_ENTRY(PlaylistEntry) entry;
} typedef PlaylistEntry;

struct MetadataEntry {
    const char *pattern;
    Metadata value;
    TAILQ_ENTRY(MetadataEntry) entry;
} typedef MetadataEntry;

struct MawConfig {
    char *art_dir;
    char *music_dir;
    TAILQ_HEAD(PlaylistEntryHead, PlaylistEntry) playlists_head;
    TAILQ_HEAD(MetadataEntryHead, MetadataEntry) metadata_head;
} typedef MawConfig;

// CLI arguments
struct MawArguments {
    char *config_path;
    size_t thread_count;
    bool verbose;
    bool dry_run;
    int av_log_level;
#ifdef MAW_TEST
    char *match_testcase;
#endif
    char *cmd;
    char **cmd_args;
    int cmd_args_count;
} typedef MawArguments;

// Compile time assertion magic
// If condition holds:
//      (void)1;
// Otherwise:
//      (void)sizeof(char[-1])
#define ASSERT(condition) ((void)sizeof(char[1 - 2 * (1 - (condition))]))

#define MAW_STRLCPY_SIZE(dst, src, size) \
    do { \
        size_t __r; \
        __r = strlcpy(dst, src, size); \
        if (__r >= size) { \
            MAW_LOGF(MAW_ERROR, "strlcpy truncation: '%s'", src); \
            goto end; \
        } \
    } while (0)

#define MAW_STRLCPY(dst, src) MAW_STRLCPY_SIZE(dst, src, sizeof(dst))

#define MAW_STRLCAT_SIZE(dst, src, size) \
    do { \
        size_t __r; \
        __r = strlcat(dst, src, size); \
        if (__r >= size) { \
            MAW_LOGF(MAW_ERROR, "strlcat truncation: '%s'", src); \
            goto end; \
        } \
    } while (0)

#define MAW_WRITE(fd, data, datasize) \
    do { \
        ssize_t write_bytes = write(fd, data, datasize); \
        if (write_bytes < 0) { \
            MAW_PERROR("write"); \
            goto end; \
        } \
        else if (write_bytes != (ssize_t)datasize) { \
            MAW_LOGF(MAW_ERROR, "short write: %zu byte(s)", write_bytes); \
            goto end; \
        } \
    } while (0)

#define MAW_STRLCAT(dst, src) MAW_STRLCAT_SIZE(dst, src, sizeof(dst))

#define STR_HAS_PREFIX(s, prefix) \
    (STR_HAS_PREFIX_SIZE(s, prefix, strlen(prefix)))
#define STR_HAS_PREFIX_SIZE(s, prefix, prefix_size) \
    (strncmp(s, prefix, prefix_size) == 0)

#define STR_EQ(target, arg) \
    (strlen(target) == strlen(arg) && strncmp(target, arg, strlen(arg)) == 0)
#define STR_CASE_EQ(target, arg) \
    (strlen(target) == strlen(arg) && \
     strncasecmp(target, arg, strlen(arg)) == 0)

#define TOSTR(arg) #arg

#define CASE_RET(a) \
    case a: \
        return TOSTR(a)

#endif // MAW_H

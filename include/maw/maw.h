#ifndef MAW_H
#define MAW_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <libavcodec/avcodec.h>
#pragma GCC diagnostic pop

#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <stdbool.h>

// Maximum number of files to handle in one invocation
#define MAW_MAX_FILES 1024

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

struct MawContext {
    const char *output_filepath;
    const MediaFile *mediafile;
    AVFormatContext *input_fmt_ctx;
    AVFormatContext *cover_fmt_ctx;
    AVFormatContext *output_fmt_ctx;
    ssize_t audio_input_stream_index;
    ssize_t video_input_stream_index;
    // Filtering variables
    AVFilterGraph *filter_graph;
    AVFilterContext *filter_buffersrc_ctx;
    AVFilterContext *filter_buffersink_ctx;
    AVCodecContext *dec_codec_ctx;
    AVCodecContext *enc_codec_ctx;
} typedef MawContext;

int maw_update(const MediaFile *mediafile) __attribute__((warn_unused_result));
void maw_mediafiles_free(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count);

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

#define MAW_STRLCAT(dst, src) MAW_STRLCAT_SIZE(dst, src, sizeof(dst))

#define STR_MATCH(target, arg) \
    (strlen(target) == strlen(arg) && strncmp(target, arg, strlen(arg)) == 0)
#define STR_CASE_MATCH(target, arg) \
    (strlen(target) == strlen(arg) && \
     strncasecmp(target, arg, strlen(arg)) == 0)

#define TOSTR(arg) #arg

#define CASE_RET(a) \
    case a: \
        return TOSTR(a)

#endif // MAW_H

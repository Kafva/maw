#ifndef MAW_H
#define MAW_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <libavcodec/avcodec.h>
#pragma GCC diagnostic pop

#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <stdbool.h>

#define CROP_ACCEPTED_WIDTH 1280
#define CROP_ACCEPTED_HEIGHT 720
#define CROP_DESIRED_WIDTH 720
#define CROP_DESIRED_HEIGHT 720

// The output should always have either:
// 1 audio stream + 1 video stream
// 1 audio stream + 0 video streams
#define AUDIO_OUTPUT_STREAM_INDEX 0
#define VIDEO_OUTPUT_STREAM_INDEX 1

// The cover policy options are mutually exclusive from one another
enum CoverPolicy {
    // Keep original cover art unless a custom `cover_path` is given (default)
    COVER_KEEP                               = 0x0,
    // Remove cover art if present
    COVER_CLEAR                              = 0x1,
    // Crop 1280x720 covers to 720x720, idempotent for 720x720 covers.
    COVER_CROP                               = 0x1 << 1,
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
    const char *filepath;
    const char *title;
    const char *album;
    const char *artist;
    const char *cover_path;
    CoverPolicy cover_policy;
    bool clean;
} typedef Metadata;

struct MawContext {
   const char *output_filepath;
   const Metadata *metadata;
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

int maw_update(const Metadata *metadata) __attribute__((warn_unused_result));

#define MAW_STRLCPY(dst, src) do {\
    size_t __r; \
    __r = strlcpy(dst, src, sizeof(dst)); \
    if (__r >= sizeof(dst)) { \
        MAW_LOGF(MAW_ERROR, "strlcpy truncation: '%s'", src); \
        goto end; \
    } \
} while (0)

#define MAW_CREATE_FILTER(r, filter_ctx, filter, name, filter_graph, args) do { \
    r = avfilter_graph_create_filter(filter_ctx, filter, name, args, NULL, filter_graph); \
    if (r != 0) { \
        MAW_AVERROR(r, "Failed to create filter"); \
        goto end; \
    } \
    if (args == NULL) { \
        MAW_LOGF(MAW_DEBUG, "Created %s filter: (no arguments)", name); \
    } \
    else { \
        MAW_LOGF(MAW_DEBUG, "Created %s filter: %s", name, args); \
    } \
} while (0)

#define NEEDS_ORIGINAL_COVER(metadata) \
    (metadata->cover_policy != COVER_CLEAR &&  \
     (metadata->cover_policy == COVER_CROP || metadata->cover_path == NULL))

#define AUDIO_INPUT_STREAM(ctx) ctx->input_fmt_ctx->streams[ctx->audio_input_stream_index]
#define VIDEO_INPUT_STREAM(ctx) ctx->input_fmt_ctx->streams[ctx->video_input_stream_index]

#define AUDIO_OUTPUT_STREAM(ctx) ctx->input_fmt_ctx->streams[AUDIO_OUTPUT_STREAM_INDEX]
#define VIDEO_OUTPUT_STREAM(ctx) ctx->input_fmt_ctx->streams[VIDEO_OUTPUT_STREAM_INDEX]

#define STR_MATCH(target, arg) (strlen(target) == strlen(arg) && \
                                strncmp(target, arg, strlen(arg)) == 0)
#define STR_CASE_MATCH(target, arg) (strlen(target) == strlen(arg) && \
                                     strncasecmp(target, arg, strlen(arg)) == 0)

#endif // MAW_H

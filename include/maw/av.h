#ifndef MAW_AV_H
#define MAW_AV_H

#include "maw/maw.h"

int maw_remux(MawContext *ctx);
void maw_free_context(MawContext *ctx);
MawContext* maw_init_context(const MediaFile *mediafile,
                             const char *output_filepath);

#define CROP_ACCEPTED_WIDTH 1280
#define CROP_ACCEPTED_HEIGHT 720
#define CROP_DESIRED_WIDTH 720
#define CROP_DESIRED_HEIGHT 720

// The output should always have either:
// 1 audio stream + 1 video stream
// 1 audio stream + 0 video streams
#define AUDIO_OUTPUT_STREAM_INDEX 0
#define VIDEO_OUTPUT_STREAM_INDEX 1

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

#endif // MAW_AV_H

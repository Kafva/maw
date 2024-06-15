#ifndef MAW_H
#define MAW_H

#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include <libavformat/avformat.h>
#include <stdbool.h>


#define CROP_ACCEPTED_WIDTH 1280
#define CROP_ACCEPTED_HEIGHT 720
#define CROP_DESIRED_WIDTH 720
#define CROP_DESIRED_HEIGHT 720

// The cover policy options are mutually exclusive from one another
enum CoverPolicy {
    // Keep original cover art unless a custom `cover_path` is given (default)
    KEEP_COVER_UNLESS_PROVIDED               = 0x0,
    // Remove cover art if present
    CLEAR_COVER                              = 0x1,
    // Crop 1280x720 covers to 720x720, idempotent for 720x720 covers.
    CROP_COVER                               = 0x1 << 1,
} typedef CoverPolicy;

enum MediaError {
    // Fallback error code for maw functions
    INTERNAL_ERROR = 50,
    // Input file has an unsupported set of streams
    UNSUPPORTED_INPUT_STREAMS = 51,
} typedef MediaError;

#define NEEDS_ORIGINAL_COVER(metadata) \
    (metadata->cover_policy != CLEAR_COVER &&  \
     (metadata->cover_policy == CROP_COVER || metadata->cover_path == NULL))

struct Metadata {
    char *title;
    char *album;
    char *artist;
    char *cover_path;
    CoverPolicy cover_policy;
    bool clear_non_core_fields;
} typedef Metadata;

#define VIDEO_INPUT_STREAM(ctx) ctx->input_fmt_ctx->streams[ctx->video_input_stream_index]
#define AUDIO_INPUT_STREAM(ctx) ctx->input_fmt_ctx->streams[ctx->audio_input_stream_index]

struct MawContext {
   const char *input_filepath;
   const char *output_filepath;
   const Metadata *metadata;
   AVFormatContext *input_fmt_ctx;
   AVFormatContext *cover_fmt_ctx;
   AVFormatContext *output_fmt_ctx;
   int audio_input_stream_index;
   int video_input_stream_index;
   // Filtering variables
   AVFilterContext *filter_buffersrc_ctx;
   AVFilterContext *filter_buffersink_ctx;
   AVCodecContext *dec_codec_ctx;
   AVCodecContext *enc_codec_ctx;
} typedef MawContext;

int maw_update(const char *, const Metadata *);

#endif // MAW_H

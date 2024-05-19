#include "maw.h"
#include "log.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

int maw_dump(const char *filepath) {
    int r;
    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *tag = NULL;

    if ((r = avformat_open_input(&fmt_ctx, filepath, NULL, NULL))) {
        MAW_PERROR(r, filepath);
        return r;
    }

    if ((r = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        MAW_PERROR(r, filepath);
        return r;
    }

    MAW_LOG(AV_LOG_INFO, filepath);
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag))) {
        printf("%s=%.32s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);

    return 0;
}

static int maw_remux(const char *input_filepath, 
                     const char *output_filepath,
                     const struct Metadata *metadata) {
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    AVPacket *pkt = NULL;

    (void)metadata;

    int r = AVERROR_UNKNOWN;

    // Create context for input file
    r = avformat_open_input(&input_fmt_ctx, input_filepath, NULL, NULL);
    if (r != 0) {
        MAW_PERROR(r, input_filepath);
        goto cleanup;
    }
    // Read input file metadata
    r = avformat_find_stream_info(input_fmt_ctx, NULL);
    if (r != 0) {
        MAW_PERROR(r, input_filepath);
        goto cleanup;
    }

    MAW_LOGF(AV_LOG_DEBUG, "%s: %d stream(s)\n", input_filepath, 
                                                 input_fmt_ctx->nb_streams);

    // Create context for output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, "m4a", output_filepath);
    if (r != 0) {
        MAW_PERROR(r, output_filepath);
        goto cleanup;
    }

    // Copy the stream from the input file to the output file
    for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
        AVStream *out_stream = avformat_new_stream(output_fmt_ctx, NULL);
        switch (input_fmt_ctx->streams[i]->codecpar->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                avcodec_parameters_copy(out_stream->codecpar, input_fmt_ctx->streams[i]->codecpar);
                break;
            case AVMEDIA_TYPE_VIDEO:
                avcodec_parameters_copy(out_stream->codecpar, input_fmt_ctx->streams[i]->codecpar);
                break;
            default:
                break;
        }
    }

    r = avio_open(&output_fmt_ctx->pb, output_filepath, AVIO_FLAG_WRITE);
    if (r < 0) {
        MAW_PERROR(r, output_filepath);
        goto cleanup;
    }

    r = avformat_write_header(output_fmt_ctx, NULL);
    if (r < 0) {
        MAW_PERROR(r, "Failed to write header information");
        goto cleanup;
    }

    av_write_trailer(output_fmt_ctx);

    r = 0;

cleanup:
    av_packet_free(&pkt);
    avformat_close_input(&input_fmt_ctx);

    if (input_fmt_ctx != NULL) {
        avformat_free_context(input_fmt_ctx);
    }
    if (output_fmt_ctx != NULL) {
        if (output_fmt_ctx->oformat != NULL && 
            !(output_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&output_fmt_ctx->pb);
        }
        avformat_free_context(output_fmt_ctx);
    }
    //av_freep(&stream_mapping);

    return r;

}

int maw_update(const char *filepath, const struct Metadata *metadata) {
    const char *output_filepath = "new.m4a";
    return maw_remux(filepath, output_filepath, metadata);
}

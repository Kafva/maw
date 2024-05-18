#include "maw.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>

int maw_init(int level) {
    av_log_set_level(level);
    return 0;
}

int maw_dump(const char *filepath) {
    int r;
    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *tag = NULL;

    if ((r = avformat_open_input(&fmt_ctx, filepath, NULL, NULL))) {
        fprintf(stderr, "Cannot open %s\n", filepath);
        return r;
    }

    if ((r = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Cannot find stream information in %s\n", filepath);
        return r;
    }

    printf("=== %s\n", filepath);
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag))) {
        printf("%s=%.32s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);

    return 0;
}

int maw_update(const char *filepath, const struct Metadata *metadata) {
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    // AVCodecContext *decode_ctx = NULL;
    // AVCodecContext *encode_ctx = NULL;
    // AVFrame *decode_frame = NULL;

    (void)metadata;

    int r = -1;

    // Open input file
    r = avformat_open_input(&input_fmt_ctx, filepath, NULL, NULL);
    if (r != 0) {
        fprintf(stderr, "Cannot open %s\n", filepath);
        goto cleanup;
    }

    r = avformat_find_stream_info(input_fmt_ctx, NULL);
    if (r != 0) {
        fprintf(stderr, "Cannot find stream information in %s\n", filepath);
        goto cleanup;
    }

    av_dump_format(input_fmt_ctx, 0, filepath, 0);

    // av_dict_set(&input_fmt_ctx->metadata, "title", metadata->title, 0);
    // if (r != 0) {
    //     goto cleanup;
    // }

    // av_dict_set(&input_fmt_ctx->metadata, "artist", metadata->artist, 0);
    // if (r != 0) {
    //     goto cleanup;
    // }




    // Open output file
    const char *output_filepath = "new.m4a";
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filepath);
    if (r != 0) {
        fprintf(stderr, "Failed to create output avformat context\n");
        goto cleanup;
    }

    // AVStream *stream = NULL;
    // const AVCodec *dec;

    // for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
    //     stream = input_fmt_ctx->streams[i];
    //     dec = avcodec_find_decoder(stream->codecpar->codec_id);
    //     AVCodecContext *codec_ctx;
    //     if (!dec) {
    //         av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
    //         return AVERROR_DECODER_NOT_FOUND;
    //     }
    //     codec_ctx = avcodec_alloc_context3(dec);
    //     if (!codec_ctx) {
    //         av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
    //         return AVERROR(ENOMEM);
    //     }
    //     r = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    //     if (r < 0) {
    //         av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
    //                "for stream #%u\n", i);
    //         return r;
    //     }

    //     /* Inform the decoder about the timebase for the packet timestamps.
    //      * This is highly recommended, but not mandatory. */
    //     codec_ctx->pkt_timebase = stream->time_base;

    //     /* Reencode video & audio and remux subtitles etc. */
    //     if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
    //             || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
    //         if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    //             codec_ctx->framerate = av_guess_frame_rate(input_fmt_ctx, stream, NULL);
    //         /* Open decoder */
    //         r = avcodec_open2(codec_ctx, dec, NULL);
    //         if (r < 0) {
    //             av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
    //             return r;
    //         }
    //     }
    //     decode_ctx = codec_ctx;

    //     decode_frame = av_frame_alloc();
    //     if (!decode_frame) {
    //         return AVERROR(ENOMEM);
    //     }
    // }


















    // if (!(output_fmt_ctx->flags & AVFMT_NOFILE)) {
    //     r = avio_open(&output_fmt_ctx->pb, filepath, AVIO_FLAG_WRITE);
    //     if (r < 0) {
    //         fprintf(stderr, "Could not open '%s' for writing", filepath);
    //         goto cleanup;
    //     }
    // }

    // if ((r = avformat_write_header(output_fmt_ctx, &(input_fmt_ctx->metadata))) != 0) {
    //     goto cleanup;
    // }
    
    r = 0;

cleanup:
    if (input_fmt_ctx != NULL) {
        avformat_close_input(&input_fmt_ctx);
        avformat_free_context(input_fmt_ctx);
    }
    if (output_fmt_ctx != NULL) {
        avformat_free_context(output_fmt_ctx);
    }

    return r;
}

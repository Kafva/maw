#include "maw.h"

#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>

static void maw_log_prefix(int level, const char *filename, int line) {
    switch (level) {
        case AV_LOG_DEBUG:
            fprintf(stderr, "\033[94mDEBUG\033[0m [%s:%d] ", filename, line);
            break;
        case AV_LOG_INFO:
            fprintf(stderr, "\033[92mINFO\033[0m [%s:%d] ", filename, line);
            break;
        case AV_LOG_WARNING:
            fprintf(stderr, "\033[93mWARN\033[0m [%s:%d] ", filename, line);
            break;
        case AV_LOG_ERROR:
            fprintf(stderr, "\033[91mERROR\033[0m [%s:%d] ", filename, line);
            break;
    }
}

void maw_logf(int level, const char *filename, int line, const char *fmt, ...) {
    va_list args;

    maw_log_prefix(level, filename, line);

    va_start(args, fmt);

    fprintf(stderr, fmt, args);

    va_end(args);
}

void maw_log(int level, const char *filename, int line, const char *msg) {
    maw_log_prefix(level, filename, line);
    write(STDERR_FILENO, msg, strlen(msg));
}

int maw_init(int level) {
    av_log_set_level(level);
    return 0;
}

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

int maw_update(const char *filepath, const struct Metadata *metadata) {
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    AVPacket *pkt = NULL;
    const AVOutputFormat *output_fmt = NULL;

    (void)metadata;

    int r = AVERROR_UNKNOWN;

    // Open input file
    r = avformat_open_input(&input_fmt_ctx, filepath, NULL, NULL);
    if (r != 0) {
        MAW_PERROR(r, filepath);
        goto cleanup;
    }

    r = avformat_find_stream_info(input_fmt_ctx, NULL);
    if (r != 0) {
        MAW_PERROR(r, filepath);
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
        MAW_PERROR(r, filepath);
        goto cleanup;
    }

    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;

    stream_mapping_size = input_fmt_ctx->nb_streams;
    stream_mapping = av_calloc(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        r = AVERROR(ENOMEM);
        MAW_PERROR(r, (const char*)NULL);
        goto cleanup;
    }

    output_fmt = output_fmt_ctx->oformat;

    for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = input_fmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        stream_mapping[i] = stream_index++;

        out_stream = avformat_new_stream(output_fmt_ctx, NULL);
        if (out_stream == NULL) {
            MAW_LOGF(AV_LOG_ERROR, "Failed to allocate output stream for '%s'\n", output_filepath);
            goto cleanup;
        }

        r = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (r < 0) {
            MAW_PERROR(r, output_filepath);
            goto cleanup;
        }
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(output_fmt_ctx, 0, output_filepath, 1);

    if (!(output_fmt->flags & AVFMT_NOFILE)) {
        r = avio_open(&output_fmt_ctx->pb, output_filepath, AVIO_FLAG_WRITE);
        if (r < 0) {
            MAW_PERROR(r, output_filepath);
            goto cleanup;
        }
    }

    r = avformat_write_header(output_fmt_ctx, NULL);
    if (r < 0) {
        MAW_PERROR(r, output_filepath);
        goto cleanup;
    }

    while (true) {
        AVStream *in_stream, *out_stream;

        r = av_read_frame(input_fmt_ctx, pkt);
        if (r < 0) {
            break;
        }

        in_stream  = input_fmt_ctx->streams[pkt->stream_index];
        if (pkt->stream_index >= stream_mapping_size ||
            stream_mapping[pkt->stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        pkt->stream_index = stream_mapping[pkt->stream_index];
        out_stream = output_fmt_ctx->streams[pkt->stream_index];


        /* copy packet */
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;


        r = av_interleaved_write_frame(output_fmt_ctx, pkt);
        /* pkt is now blank (av_interleaved_write_frame() takes ownership of
         * its contents and resets pkt), so that no unreferencing is necessary.
         * This would be different if one used av_write_frame(). */
        if (r < 0) {
            MAW_PERROR(r, output_filepath);
            break;
        }
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
        if (output_fmt != NULL && !(output_fmt->flags & AVFMT_NOFILE)) {
            avio_closep(&output_fmt_ctx->pb);
        }
        avformat_free_context(output_fmt_ctx);
    }
    av_freep(&stream_mapping);

    return r;
}

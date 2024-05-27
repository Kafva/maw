#include "maw.h"
#include "log.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/avassert.h>

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
    int stream_index = 0;
    // Mapping from output stream index -> input stream index
    int *stream_mapping = NULL;
    int nb_streams = 0;

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

    // Setup stream mapping
    av_assert0(input_fmt_ctx->nb_streams < INT_MAX);
    nb_streams = input_fmt_ctx->nb_streams;

    stream_mapping = av_calloc(nb_streams, sizeof(*stream_mapping));
    if (stream_mapping == NULL) {
        r = AVERROR(ENOMEM);
        MAW_PERROR(r, "Out of memory");
        goto cleanup;
    }

    pkt = av_packet_alloc();
    if (pkt == NULL) {
        MAW_LOG(AV_LOG_ERROR, "Failed to allocate packet");
        goto cleanup;
    }


    MAW_LOGF(AV_LOG_DEBUG, "%s: %d stream(s)\n", input_filepath, nb_streams);

    // Create context for output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, "mp4", output_filepath);
    if (r != 0) {
        MAW_PERROR(r, output_filepath);
        goto cleanup;
    }

    // Copy the stream from the input file to the output file
    for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
        AVStream *out_stream = avformat_new_stream(output_fmt_ctx, NULL);

        switch (input_fmt_ctx->streams[i]->codecpar->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
            case AVMEDIA_TYPE_VIDEO:
                r = avcodec_parameters_copy(out_stream->codecpar, input_fmt_ctx->streams[i]->codecpar);
                break;
            default:
                stream_mapping[i] = -1;
                continue;
        }

        if (r != 0) {
            MAW_PERROR(r, output_filepath);
            goto cleanup;
        }
        stream_mapping[i] = stream_index++;
        out_stream->codecpar->codec_tag = 0;
    }

    r = avio_open(&output_fmt_ctx->pb, output_filepath, AVIO_FLAG_WRITE);
    if (r != 0) {
        MAW_PERROR(r, output_filepath);
        goto cleanup;
    }

    r = avformat_write_header(output_fmt_ctx, NULL);
    if (r != 0) {
        MAW_PERROR(r, "Failed to write header");
        goto cleanup;
    }

    while (true) {
        AVStream *in_stream, *out_stream;

        r = av_read_frame(input_fmt_ctx, pkt);

        if (r != 0) {
            break;
        }

        in_stream = input_fmt_ctx->streams[pkt->stream_index];

        // Bad stream index, skip
        if (pkt->stream_index >= nb_streams || 
            stream_mapping[pkt->stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        // Set the stream index in the output pkt to the corresponding index
        // from the input stream
        pkt->stream_index = stream_mapping[pkt->stream_index];

        // Rescale (?)
        out_stream = output_fmt_ctx->streams[pkt->stream_index];
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;

        r = av_interleaved_write_frame(output_fmt_ctx, pkt);
        // pkt is now blank (av_interleaved_write_frame() takes ownership of
        // its contents and resets pkt), so that no unreferencing is necessary.
        // This would be different if one used av_write_frame().
        if (r != 0) {
            MAW_PERROR(r, "Failed to mux packet");
            break;
        }
    }

    r = av_write_trailer(output_fmt_ctx);
    if (r != 0) {
        MAW_PERROR(r, "Failed to write trailer");
        goto cleanup;
    }

    MAW_LOG(AV_LOG_INFO, "OK\n");

cleanup:
    av_packet_free(&pkt);
    av_freep(&stream_mapping);
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

    return r;

}

int maw_update(const char *filepath, const struct Metadata *metadata) {
    const char *output_filepath = "new.m4a";
    return maw_remux(filepath, output_filepath, metadata);
}

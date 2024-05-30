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

    MAW_LOG(MAW_INFO, filepath);
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag))) {
        printf("%s=%.32s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);

    return 0;
}

// See "Stream copy" section of ffmpeg(1), that is what we are doing
static int maw_remux(const char *input_filepath,
                     const char *output_filepath,
                     const struct Metadata *metadata) {
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    AVPacket *pkt = NULL;
    int stream_index = 0;
    // Mapping: [input stream index] -> [output stream index]
    // We could keep the exact stream indices from the input but
    // that would prevent us from e.g. dropping a subtitle stream.
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
        MAW_LOG(MAW_ERROR, "Failed to allocate packet");
        goto cleanup;
    }

    MAW_LOGF(MAW_DEBUG, "%s: %d stream(s)\n", input_filepath, nb_streams);

    // Create context for output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filepath);
    if (r != 0) {
        MAW_PERROR(r, output_filepath);
        goto cleanup;
    }

    for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
        switch (input_fmt_ctx->streams[i]->codecpar->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
            case AVMEDIA_TYPE_VIDEO:
                // Create an output stream for each OK input stream
                output_stream = avformat_new_stream(output_fmt_ctx, NULL);
                input_stream = input_fmt_ctx->streams[i];

                // Stream copy from the input stream onto the output
                r = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);

                if (r != 0) {
                    MAW_PERROR(r, output_filepath);
                    goto cleanup;
                }

                // Set matching disposition on output stream
                // We get a deprecation warning if we do not do this since video
                // streams with an attached_pic do not have timestamps
                //  'Timestamps are unset in a packet for stream...'
                output_stream->disposition = input_stream->disposition;

                //output_stream->start_time = input_stream->start_time;
                //output_stream->duration = input_stream->duration;
                //output_stream->time_base = input_stream->time_base;
                //output_stream->nb_frames = input_stream->nb_frames;

                // Update the mapping
                stream_mapping[i] = stream_index++;
                break;
            default:
                stream_mapping[i] = -1;
                break;
        }
    }

    // FFmpeg gives output like this during stream copying
    // It shows us
    // Stream mapping:
    //   Stream #0:1 -> #0:0 (copy)
    //   Stream #0:0 -> #0:1 (copy)
    for (int i = 0; i < nb_streams; i++) {
        // We only work on one file
        int input_file_idx = 0;
        int output_file_idx = 0;
        MAW_LOGF(MAW_INFO, "Stream #%d:%d -> #%d:%d (copy)\n", input_file_idx,
                                                               i,
                                                               output_file_idx,
                                                               stream_mapping[i]);
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
        r = av_read_frame(input_fmt_ctx, pkt);
        if (r != 0) {
            break; // No more frames
        }

        // Bad stream index, skip
        if (pkt->stream_index >= nb_streams ||
            stream_mapping[pkt->stream_index] < 0) {
            // BAD packet 😠
            av_packet_unref(pkt);
            continue;
        }

        // Input and output stream for the current packet
        input_stream = input_fmt_ctx->streams[pkt->stream_index];
        output_stream = output_fmt_ctx->streams[stream_mapping[pkt->stream_index]];

        // The pkt will have the stream_index set to the stream index in the
        // input file. Remap it to the correct stream_index in the output file.
        pkt->stream_index = stream_mapping[pkt->stream_index];

        // Rescale (?)
        av_packet_rescale_ts(pkt, 
                             input_stream->time_base, 
                             output_stream->time_base);
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

    MAW_LOG(MAW_INFO, "OK\n");

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

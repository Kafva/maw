#include "maw.h"
#include "log.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/avassert.h>
#include <sys/errno.h>
#include <unistd.h>
#include <fcntl.h>

static int readfile(const char *filepath, char *out, size_t outsize) {
    FILE *fp;
    size_t read_bytes;
    int r = 1;

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        MAW_PERROR(filepath);
        return 1;
    }
    
    read_bytes = fread(out, 1, outsize, fp);
    if (read_bytes <= 0) {
        MAW_LOGF(MAW_ERROR, "%s: empty", filepath);
        goto end;
    }
    else if (read_bytes == outsize) {
        MAW_LOGF(MAW_ERROR, "%s: too large", filepath);
        goto end;
    }

    r = 0;
end:
    fclose(fp);
    return r;
}

static int maw_copy_metadata_fields(AVFormatContext *fmt_ctx,
                                    const struct Metadata *metadata) {
    int r = AVERROR_UNKNOWN;
    r = av_dict_set(&fmt_ctx->metadata, "title", metadata->title, 0);
    if (r != 0) {
        goto end;
    }

    r = av_dict_set(&fmt_ctx->metadata, "artist", metadata->artist, 0);
    if (r != 0) {
        goto end;
    }

    r = av_dict_set(&fmt_ctx->metadata, "album", metadata->album, 0);
    if (r != 0) {
        goto end;
    }

    r = 0;
end:
    return r;
}

/**
 * @return 0 on success, negative AVERROR code on failure.
 */
static int maw_set_metadata(AVFormatContext *input_fmt_ctx,
                            AVFormatContext *output_fmt_ctx,
                            const struct Metadata *metadata,
                            const int policy) {
    int r = AVERROR_UNKNOWN;
    const AVDictionaryEntry *entry = NULL;

    if (policy & KEEP_ALL_FIELDS) {
        // Keep the metadata as is
        r = av_dict_copy(&output_fmt_ctx->metadata, input_fmt_ctx->metadata, 0);
        if (r != 0) {
            goto end;
        }
    }
    else if (policy & KEEP_CORE_FIELDS) {
        // Keep some of the metadata
        while ((entry = av_dict_iterate(input_fmt_ctx->metadata, entry))) {
            r = av_dict_set(&output_fmt_ctx->metadata, entry->key, entry->value, 0);
            if (r != 0) {
                goto end;
            }
        }
    } else {
        // Do not keep anything
    }

    // Set custom values
    if (metadata != NULL) {
        r = maw_copy_metadata_fields(output_fmt_ctx, metadata);
        if (r != 0) {
            goto end;
        }
    }

    r = 0;
end:
    return r;
}

// See "Stream copy" section of ffmpeg(1), that is what we are doing
static int maw_remux(const char *input_filepath,
                     const char *output_filepath,
                     const struct Metadata *metadata,
                     const int policy) {
    int r = AVERROR_UNKNOWN;
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    AVPacket *pkt = NULL;
    enum AVMediaType codec_type;
    int stream_index = 0;
    // Mapping: [input stream index] -> [output stream index]
    // We could keep the exact stream indices from the input but
    // that would prevent us from e.g. dropping a subtitle stream.
    int *stream_mapping = NULL;
    int nb_streams = 0;
    int nb_audio_streams = 0;
    int nb_video_streams = 0;
    char cover_data[BUFSIZ];

    // Create context for input file
    r = avformat_open_input(&input_fmt_ctx, input_filepath, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, input_filepath);
        goto end;
    }
    // Read input file metadata
    r = avformat_find_stream_info(input_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, input_filepath);
        goto end;
    }

    // Setup stream mapping
    av_assert0(input_fmt_ctx->nb_streams < INT_MAX);
    nb_streams = input_fmt_ctx->nb_streams;

    stream_mapping = av_calloc(nb_streams, sizeof(*stream_mapping));
    if (stream_mapping == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Out of memory");
        goto end;
    }

    pkt = av_packet_alloc();
    if (pkt == NULL) {
        MAW_LOG(MAW_ERROR, "Failed to allocate packet");
        goto end;
    }

    MAW_LOGF(MAW_DEBUG, "%s: %d stream(s)\n", input_filepath, nb_streams);

    // Create context for output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filepath);
    if (r != 0) {
        MAW_AVERROR(r, output_filepath);
        goto end;
    }

    for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
        codec_type = input_fmt_ctx->streams[i]->codecpar->codec_type;
        switch (codec_type) {
            case AVMEDIA_TYPE_AUDIO:
            case AVMEDIA_TYPE_VIDEO:
                if (codec_type == AVMEDIA_TYPE_VIDEO)
                    nb_video_streams += 1;
                else
                    nb_audio_streams += 1;

                // Create an output stream for each OK input stream
                output_stream = avformat_new_stream(output_fmt_ctx, NULL);
                input_stream = input_fmt_ctx->streams[i];

                // Stream copy from the input stream onto the output
                r = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);

                if (r != 0) {
                    MAW_AVERROR(r, output_filepath);
                    goto end;
                }

                // Set matching disposition on output stream
                // We get a deprecation warning if we do not do this since video
                // streams with an attached_pic do not have timestamps
                //  'Timestamps are unset in a packet for stream...'
                output_stream->disposition = input_stream->disposition;

                // Update the mapping
                stream_mapping[i] = stream_index++;
                break;
            default:
                stream_mapping[i] = -1;
                break;
        }
    }

    // The metadata for artist etc. is in the AVFormatContext, streams also have
    // a metadata field but these contain other stuff, e.g. audio streams can
    // have 'language' and 'handler_name'
    r = maw_set_metadata(input_fmt_ctx, output_fmt_ctx, metadata, policy);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to copy metadata");
        goto end;
    }

    if (metadata->cover_path != NULL) {
        //r = readfile(metadata->cover_path, cover_data, sizeof(cover_data));
        //if (r != 0) {
        //    goto end;
        //}

        if (nb_video_streams == 0) {
            // TODO Add a new stream
            // read with avformat_open_input(&formatContext, "cover.jpg", NULL, NULL)
        }
    }

    // FFmpeg gives output like this during stream copying
    // It shows us
    // Stream mapping:
    //   Stream #0:1 -> #0:0 (copy)
    //   Stream #0:0 -> #0:1 (copy)
    for (int i = 0; i < nb_streams; i++) {
        MAW_LOGF(MAW_DEBUG, "Stream #0:%d -> #0:%d (copy)\n", i,
                                                             stream_mapping[i]);
    }

    if (nb_audio_streams != 1) {
        r = AVERROR_UNKNOWN;
        MAW_LOGF(MAW_ERROR, "There should be exactly one audio stream, found %d\n", nb_audio_streams);
        goto end;
    }

    if (nb_video_streams > 1) {
        r = AVERROR_UNKNOWN;
        MAW_LOGF(MAW_ERROR, "There should not be more than one video stream, found %d\n", nb_video_streams);
        goto end;
    }

    r = avio_open(&output_fmt_ctx->pb, output_filepath, AVIO_FLAG_WRITE);
    if (r != 0) {
        MAW_AVERROR(r, output_filepath);
        goto end;
    }

    r = avformat_write_header(output_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to write header");
        goto end;
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
            MAW_AVERROR(r, "Failed to mux packet");
            break;
        }

        // This warning: 'Encoder did not produce proper pts, making some up.'
        // appears for packets in cover art streams (since they do not have a
        // pts value set), its harmless, more info on pts:
        // http://dranger.com/ffmpeg/tutorial05.html
    }

    r = av_write_trailer(output_fmt_ctx);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to write trailer");
        goto end;
    }

end:
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

int maw_update(const char *filepath,
               const struct Metadata *metadata,
               const int policy) {
    int r = AVERROR_UNKNOWN;
    char tmpfile[] = "/tmp/maw.XXXXX.m4a";
    int tmphandle = mkstemps(tmpfile, sizeof(".m4a") - 1);

    if (tmphandle < 0) {
         MAW_PERROR(tmpfile);
         goto end;
    }
    (void)close(tmphandle);

    MAW_LOGF(MAW_DEBUG, "%s -> %s\n", filepath, tmpfile);

    r = maw_remux(filepath, tmpfile, metadata, policy);
    if (r != 0) {
        goto end;
    }

    r = rename(tmpfile, filepath);
    if (r < 0) {
         MAW_PERROR(tmpfile);
         goto end;
    }

end:
    (void)unlink(tmpfile);
    return r;
}

#ifdef MAW_TEST

// TODO verify cover
bool maw_verify(const char *filepath,
                const struct Metadata *metadata,
                const int policy) {
    bool ok = false;
    int r;
    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *entry = NULL;

    if ((r = avformat_open_input(&fmt_ctx, filepath, NULL, NULL))) {
        MAW_AVERROR(r, filepath);
        goto end;
    }

    if ((r = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        MAW_AVERROR(r, filepath);
        goto end;
    }

    while ((entry = av_dict_iterate(fmt_ctx->metadata, entry))) {
        if (strcmp(entry->key, "title") == 0) {
            if (strcmp(entry->value, metadata->title) != 0)
                goto end;
        }
        else if (strcmp(entry->key, "artist") == 0) {
            if (strcmp(entry->value, metadata->artist) != 0)
                goto end;
        }
        else if (strcmp(entry->key, "album") == 0) {
            if (strcmp(entry->value, metadata->album) != 0)
                goto end;
        }
        else if (policy & KEEP_ALL_FIELDS) {
            // There should be no other fields
            goto end;
        }
    }

    ok = true;
end:
    avformat_close_input(&fmt_ctx);
    return ok;
}

#endif


#include "maw.h"
#include "log.h"
#include "util.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/avassert.h>
#include <unistd.h>


static int maw_demux_cover(AVFormatContext **cover_fmt_ctx,
                           AVFormatContext *output_fmt_ctx,
                           const struct Metadata *metadata) {
    int r = AVERROR_UNKNOWN;
    AVStream *output_stream = NULL;
    enum AVMediaType codec_type;

    // Demux the input file
    r = avformat_open_input(cover_fmt_ctx, metadata->cover_path, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, metadata->cover_path);
        goto end;
    }
    r = avformat_find_stream_info(*cover_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, metadata->cover_path);
        goto end;
    }

    if ((*cover_fmt_ctx)->nb_streams == 0) {
        MAW_LOGF(MAW_ERROR, "%s: cover has no input streams\n", metadata->cover_path);
        r = AVERROR_UNKNOWN;
        goto end;
    }
    if ((*cover_fmt_ctx)->nb_streams > 1) {
        MAW_LOGF(MAW_ERROR, "%s: cover has more than one input stream\n", metadata->cover_path);
        r = AVERROR_UNKNOWN;
        goto end;
    }
    codec_type = (*cover_fmt_ctx)->streams[0]->codecpar->codec_type;
    if (codec_type != AVMEDIA_TYPE_VIDEO) {
        MAW_LOGF(MAW_ERROR, "%s: cover does not contain a video stream (found %s)\n",
                 metadata->cover_path,
                 av_get_media_type_string(codec_type));
        r = AVERROR_UNKNOWN;
        goto end;
    }

    // Always create a new video stream for the output
    output_stream = avformat_new_stream(output_fmt_ctx, NULL);

    r = avcodec_parameters_copy(output_stream->codecpar,
                                (*cover_fmt_ctx)->streams[0]->codecpar);
    if (r != 0) {
        MAW_AVERROR(r, metadata->cover_path);
        goto end;
    }

    output_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;

    r = 0;
end:
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
            if (strcmp(entry->key, "title") != 0 &&
                strcmp(entry->key, "artist") != 0 &&
                strcmp(entry->key, "album") != 0) {
                continue;
            }
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

// Video streams will only be demuxed if they are needed by the current policy
static int maw_demux_media(const char *input_filepath,
                           const char *output_filepath,
                           const int policy,
                           AVFormatContext **input_fmt_ctx,
                           AVFormatContext **output_fmt_ctx,
                           int *audio_input_stream_index,
                           int *video_input_stream_index) {
    int r = AVERROR_UNKNOWN;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    enum AVMediaType codec_type;
    int stream_index = 0;

    // Create context for input file
    r = avformat_open_input(input_fmt_ctx, input_filepath, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, input_filepath);
        goto end;
    }
    // Read input file metadata
    r = avformat_find_stream_info(*input_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, input_filepath);
        goto end;
    }

    // Create context for output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(output_fmt_ctx, NULL, NULL, output_filepath);
    if (r != 0) {
        MAW_AVERROR(r, output_filepath);
        goto end;
    }

    // Always add the audio stream first, i.e. output stream 0 will always be the
    // audio stream!
    for (unsigned int i = 0; i < (*input_fmt_ctx)->nb_streams; i++) {
        codec_type = (*input_fmt_ctx)->streams[i]->codecpar->codec_type;
        if (codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        if (*audio_input_stream_index != -1) {
            r = AVERROR_UNKNOWN;
            MAW_LOG(MAW_ERROR, "Found more than one audio stream\n");
            goto end;
        }
        *audio_input_stream_index = i;

        // Create an output stream for each OK input stream
        output_stream = avformat_new_stream(*output_fmt_ctx, NULL);
        input_stream = (*input_fmt_ctx)->streams[i];

        // Stream copy from the input stream onto the output
        r = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);

        if (r != 0) {
            MAW_AVERROR(r, output_filepath);
            goto end;
        }
    }

    for (unsigned int i = 0; i < (*input_fmt_ctx)->nb_streams; i++) {
        codec_type = (*input_fmt_ctx)->streams[i]->codecpar->codec_type;
        if (codec_type != AVMEDIA_TYPE_VIDEO) {
            if (codec_type != AVMEDIA_TYPE_AUDIO)
                MAW_LOGF(MAW_DEBUG, "Skipping input stream #%d\n", i);
            continue;
        }

        if (*video_input_stream_index != -1) {
            MAW_LOGF(MAW_WARN, "%s: Video input stream #%u (ignored)\n", input_filepath, i);
            continue;
        }
        *video_input_stream_index = i;

        // Do not demux the video stream if the policy does not require the
        // original image stream.
        if (!POLICY_NEEDS_ORIGINAL_COVER(policy)) {
            continue;
        }

        output_stream = avformat_new_stream(*output_fmt_ctx, NULL);
        input_stream = (*input_fmt_ctx)->streams[i];
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
    }

    MAW_LOGF(MAW_DEBUG, "%s: Audio input stream #%d\n", input_filepath,
                                                        *audio_input_stream_index);

    if (*video_input_stream_index != -1) {
        if (POLICY_NEEDS_ORIGINAL_COVER(policy)) {
            MAW_LOGF(MAW_DEBUG, "%s: Video input stream #%d\n", input_filepath,
                                                                *video_input_stream_index);
        }
        else {
            MAW_LOGF(MAW_DEBUG, "%s: Video input stream #%d (ignored)\n", input_filepath,
                                                                          *video_input_stream_index);
        }
    }
    else {
        MAW_LOGF(MAW_DEBUG, "%s: Video input stream (none)\n", input_filepath);
    }

end:
    return r;
}

static int maw_mux(const char *output_filepath,
                   const int policy,
                   AVFormatContext *input_fmt_ctx,
                   AVFormatContext *cover_fmt_ctx,
                   AVFormatContext *output_fmt_ctx,
                   int audio_input_stream_index,
                   int video_input_stream_index) {
    int r = AVERROR_UNKNOWN;
    int output_stream_index = -1;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    AVPacket *pkt = NULL;

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

    pkt = av_packet_alloc();
    if (pkt == NULL) {
        MAW_LOG(MAW_ERROR, "Failed to allocate packet");
        goto end;
    }

    // Mux streams from input file
    while (true) {
        r = av_read_frame(input_fmt_ctx, pkt);
        if (r != 0) {
            break; // No more frames
        }
        if (pkt->stream_index < 0 || pkt->stream_index >= (int)input_fmt_ctx->nb_streams) {
            MAW_LOGF(MAW_ERROR, "Invalid stream index: #%d\n", pkt->stream_index);
            goto end;
        }

        input_stream = input_fmt_ctx->streams[pkt->stream_index];

        if (pkt->stream_index == audio_input_stream_index) {
            // Audio stream
            output_stream_index = 0;
        }
        else if (pkt->stream_index == video_input_stream_index) {
            if (!POLICY_NEEDS_ORIGINAL_COVER(policy)) {
                // Skip original video stream
                continue;
            }
            // Video stream to keep
            output_stream_index = 1;
        }
        else {
            MAW_LOGF(MAW_DEBUG, "%s input stream #%d: packet ignored\n",
                     av_get_media_type_string(input_stream->codecpar->codec_type), 
                     pkt->stream_index);
            continue;
        }

        output_stream = output_fmt_ctx->streams[output_stream_index];

        // The pkt will have the stream_index set to the stream index in the
        // input file. Remap it to the correct stream_index in the output file.
        pkt->stream_index = output_stream_index;

        if (pkt->stream_index == audio_input_stream_index) {
            av_packet_rescale_ts(pkt, input_stream->time_base,
                                      output_stream->time_base);
            pkt->pos = -1;
        }

        // The pkt passed to this function is automatically freed
        r = av_interleaved_write_frame(output_fmt_ctx, pkt);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to mux packet");
            break;
        }

        // This warning: 'Encoder did not produce proper pts, making some up.'
        // appears for packets in cover art streams (since they do not have a
        // pts value set), its harmless, more info on pts:
        // http://dranger.com/ffmpeg/tutorial05.html
    }

    // Mux streams from cover
    while (cover_fmt_ctx != NULL) {
        r = av_read_frame(cover_fmt_ctx, pkt);
        if (r != 0) {
            break; // No more frames
        }

        if (pkt->stream_index != 0) {
            r = AVERROR_UNKNOWN;
            MAW_LOGF(MAW_ERROR, "Unexpected packet from cover stream #%d\n",
                     pkt->stream_index);
            goto end;
        }

        output_stream_index = 1;

        // Input and output stream for the current packet
        input_stream = cover_fmt_ctx->streams[pkt->stream_index];
        output_stream = output_fmt_ctx->streams[output_stream_index];

        pkt->stream_index = output_stream_index;
        pkt->pos = -1;

        r = av_interleaved_write_frame(output_fmt_ctx, pkt);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to mux packet");
            break;
        }
    }

    r = av_write_trailer(output_fmt_ctx);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to write trailer");
        goto end;
    }

end:
    av_packet_free(&pkt);
    return r;
}


// See "Stream copy" section of ffmpeg(1), that is what we are doing
// The output should always have either:
// 1 audio stream + 1 video stream
// 1 audio stream + 0 video streams
static int maw_remux(const char *input_filepath,
                     const char *output_filepath,
                     const struct Metadata *metadata,
                     const int policy) {
    int r;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    AVFormatContext *cover_fmt_ctx = NULL;
    int audio_input_stream_index = -1;
    int video_input_stream_index = -1;

    r = maw_demux_media(input_filepath,
                        output_filepath,
                        policy,
                        &input_fmt_ctx,
                        &output_fmt_ctx,
                        &audio_input_stream_index,
                        &video_input_stream_index);
    if (r != 0)
        goto end;

    if (metadata->cover_path != NULL && strlen(metadata->cover_path) > 0) {
        r = maw_demux_cover(&cover_fmt_ctx, output_fmt_ctx, metadata);
        if (r != 0)
            goto end;
    }

    // The metadata for artist etc. is in the AVFormatContext, streams also have
    // a metadata field but these contain other stuff, e.g. audio streams can
    // have 'language' and 'handler_name'
    r = maw_set_metadata(input_fmt_ctx, output_fmt_ctx, metadata, policy);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to copy metadata");
        goto end;
    }

    r = maw_mux(output_filepath,
                policy,
                input_fmt_ctx,
                cover_fmt_ctx,
                output_fmt_ctx,
                audio_input_stream_index,
                video_input_stream_index);
    if (r != 0)
        goto end;

end:
    avformat_close_input(&input_fmt_ctx);
    if (input_fmt_ctx != NULL) {
        avformat_free_context(input_fmt_ctx);
    }

    avformat_close_input(&cover_fmt_ctx);
    if (cover_fmt_ctx != NULL) {
        avformat_free_context(cover_fmt_ctx);
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
    if (r != 0) {
         MAW_PERROR(tmpfile);
         goto end;
    }

end:
    (void)unlink(tmpfile);
    return r;
}

#ifdef MAW_TEST

bool maw_verify(const char *filepath,
                const struct Metadata *metadata,
                const int policy) {
    bool ok = false;
    bool expect_cover = false;
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

    // Verify metadata
    while ((entry = av_dict_iterate(fmt_ctx->metadata, entry))) {
        if (strcmp(entry->key, "title") == 0) {
            if (!LHS_EMPTY_OR_EQ(metadata->title, entry->value))
                goto end;
        }
        else if (strcmp(entry->key, "artist") == 0) {
            if (!LHS_EMPTY_OR_EQ(metadata->artist, entry->value))
                goto end;
        }
        else if (strcmp(entry->key, "album") == 0) {
            if (!LHS_EMPTY_OR_EQ(metadata->album, entry->value))
                goto end;
        }
        else if (strcmp(entry->key, "major_brand") != 0 &&
                 strcmp(entry->key, "minor_version") != 0 &&
                 strcmp(entry->key, "compatible_brands") != 0 &&
                 strcmp(entry->key, "encoder") != 0 &&
                 !(policy & KEEP_ALL_FIELDS)) {
            // There should be no other fields
            goto end;
        }
    }

    expect_cover = metadata->cover_path != NULL ||
                   POLICY_NEEDS_ORIGINAL_COVER(policy);

    if (expect_cover) {
        ok = maw_verify_cover(fmt_ctx, filepath, metadata, policy);

    } else {
        if (fmt_ctx->nb_streams != 1) {
            MAW_LOGF(MAW_ERROR, "%s: Expected one stream: found %u\n",
                     filepath, fmt_ctx->nb_streams);
            goto end;
        }
        ok = true;
    }

end:
    avformat_close_input(&fmt_ctx);
    return ok;
}

bool maw_verify_cover(const AVFormatContext *fmt_ctx,
                      const char *filepath,
                      const struct Metadata *metadata,
                      const int policy) {
    int r;
    char cover_data[BUFSIZ];
    AVStream *stream = NULL;
    int read_bytes;
    bool ok = false;

    read_bytes = (int)readfile(metadata->cover_path,
                               cover_data,
                               sizeof cover_data);
    if (read_bytes == 0) {
        goto end;
    }

    if (fmt_ctx->nb_streams != 2) {
        MAW_LOGF(MAW_ERROR, "%s: Expected two streams: found %u\n",
                 filepath, fmt_ctx->nb_streams);
        goto end;
    }

    stream = fmt_ctx->streams[1];
    if (stream->attached_pic.data == NULL) {
        MAW_LOGF(MAW_ERROR, "%s: video stream is empty\n", filepath);
        goto end;
    }
    if (stream->attached_pic.size != read_bytes) {
        MAW_LOGF(MAW_ERROR, "%s: incorrect cover size: %d != %d\n",
                 metadata->cover_path, stream->attached_pic.size, read_bytes);
        goto end;
    }

    r = memcmp(stream->attached_pic.data, cover_data, (size_t)read_bytes);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "%s: cover data does not match\n",
                 metadata->cover_path);
        goto end;
    }

    ok = true;
end:
    return ok;
}

#endif


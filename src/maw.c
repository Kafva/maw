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

static int maw_demux_cover(AVFormatContext **cover_fmt_ctx, 
                           AVFormatContext *output_fmt_ctx, 
                           const struct Metadata *metadata, 
                           int policy,
                           int video_input_stream_index) {
    int r = AVERROR_UNKNOWN;
    AVStream *cover_stream = NULL;
    AVStream *output_stream = NULL;
    enum AVMediaType codec_type;

    if (metadata->cover_path == NULL || strlen(metadata->cover_path) == 0) {
        // No new cover configured in metadata
        r = 0;
        goto end;
    }

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

    cover_stream = avformat_new_stream(*cover_fmt_ctx, NULL);

    if (video_input_stream_index == -1) {
        // Create a new video stream
        output_stream = avformat_new_stream(output_fmt_ctx, NULL);
    }
    else {
        // Reuse existing video stream
        output_stream = output_fmt_ctx->streams[1];
    }

    cover_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;
    r = avcodec_parameters_copy(output_stream->codecpar, cover_stream->codecpar);
    if (r != 0) {
        MAW_AVERROR(r, metadata->cover_path);
        goto end;
    }

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

static int maw_demux_media(const char *input_filepath,
                     const char *output_filepath,
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

    MAW_LOGF(MAW_DEBUG, "%s: %u input stream(s)\n", input_filepath, 
                                                    (*input_fmt_ctx)->nb_streams);

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
            r = AVERROR_UNKNOWN;
            MAW_LOG(MAW_ERROR, "Found more than one video stream\n");
            goto end;
        }
        *video_input_stream_index = i;

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

end:
    // Cleanup by caller
    return r;
}

static int maw_mux(const char *output_filepath,
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

        // Skip everything except the audio and video stream from the input
        if (pkt->stream_index != audio_input_stream_index &&
            pkt->stream_index != video_input_stream_index) {
            av_packet_unref(pkt);
            continue;
        }
        // Skip original cover data if a cover context was provided
        if (pkt->stream_index == video_input_stream_index && cover_fmt_ctx != NULL) {
            MAW_LOGF(MAW_DEBUG, "Skipping existing video stream #%d in input\n", pkt->stream_index);
            
            av_packet_unref(pkt);
            continue;
        }

        // Audio stream is always the first stream (for our demuxer)
        output_stream_index = pkt->stream_index == audio_input_stream_index ? 0 : 1;

        // Input and output stream for the current packet
        input_stream = input_fmt_ctx->streams[pkt->stream_index];
        output_stream = output_fmt_ctx->streams[output_stream_index];

        // The pkt will have the stream_index set to the stream index in the
        // input file. Remap it to the correct stream_index in the output file.
        pkt->stream_index = output_stream_index;

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

    // Mux streams from cover
    while (cover_fmt_ctx != NULL) {
        r = av_read_frame(cover_fmt_ctx, pkt);
        if (r != 0) {
            break; // No more frames
        }

        if (pkt->stream_index != 0) {
            // BAD packet 😠
            av_packet_unref(pkt);
            continue;
        }

        output_stream_index = 1;

        // Input and output stream for the current packet
        input_stream = cover_fmt_ctx->streams[pkt->stream_index];
        output_stream = output_fmt_ctx->streams[output_stream_index];

        pkt->stream_index = output_stream_index;
        //pkt->pos = -1;

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
    // The output should always have either:
    // 1 audio stream + 1 video stream
    // 1 audio stream + 0 video streams

    r = maw_demux_media(input_filepath, output_filepath, 
                  &input_fmt_ctx, &output_fmt_ctx, 
                  &audio_input_stream_index,
                  &video_input_stream_index);
    if (r != 0)
        goto end;

    r = maw_demux_cover(&cover_fmt_ctx, 
                        output_fmt_ctx, 
                        metadata, 
                        policy,
                        video_input_stream_index);
    if (r != 0)
        goto end;


    // The metadata for artist etc. is in the AVFormatContext, streams also have
    // a metadata field but these contain other stuff, e.g. audio streams can
    // have 'language' and 'handler_name'
    r = maw_set_metadata(input_fmt_ctx, output_fmt_ctx, metadata, policy);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to copy metadata");
        goto end;
    }

    r = maw_mux(output_filepath, 
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


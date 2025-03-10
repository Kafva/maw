#include "maw/av.h"
#include "maw/log.h"
#include "maw/utils.h"

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avassert.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>

static int maw_av_demux_picture_file(MawAVContext *ctx);
static int maw_av_filter_crop_cover(MawAVContext *ctx);
static int maw_av_copy_metadata_fields(AVFormatContext *ctx,
                                       const MediaFile *mediafile);
static int maw_av_metadata_check(MawAVContext *ctx);
static int maw_av_cover_check_crop(MawAVContext *ctx);
static int maw_av_cover_check(MawAVContext *ctx);
static int maw_av_set_metadata(MawAVContext *ctx);
static int maw_av_demux(MawAVContext *ctx);
static int maw_av_mux(MawAVContext *ctx);
static int maw_av_init_dec_context(MawAVContext *ctx);
static int maw_av_init_enc_context(MawAVContext *ctx);

////////////////////////////////////////////////////////////////////////////////

static int maw_av_demux_picture_file(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    AVStream *output_stream = NULL;
    enum AVMediaType codec_type;

    // Demux the input file
    r = avformat_open_input(&(ctx->cover_fmt_ctx),
                            ctx->mediafile->metadata->cover_path, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->metadata->cover_path, NULL);
        goto end;
    }
    r = avformat_find_stream_info(ctx->cover_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->metadata->cover_path, NULL);
        goto end;
    }

    if (ctx->cover_fmt_ctx->nb_streams == 0) {
        MAW_LOGF(MAW_ERROR, "%s: Cover has no input streams",
                 ctx->mediafile->metadata->cover_path);
        r = RESULT_UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }
    if (ctx->cover_fmt_ctx->nb_streams > 1) {
        MAW_LOGF(MAW_ERROR, "%s: Cover has more than one input stream",
                 ctx->mediafile->metadata->cover_path);
        r = RESULT_UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }
    codec_type = ctx->cover_fmt_ctx->streams[0]->codecpar->codec_type;
    if (codec_type != AVMEDIA_TYPE_VIDEO) {
        MAW_LOGF(MAW_ERROR,
                 "%s: Cover does not contain a video stream (found %s)",
                 ctx->mediafile->metadata->cover_path,
                 av_get_media_type_string(codec_type));
        r = RESULT_UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }

    output_stream = avformat_new_stream(ctx->output_fmt_ctx, NULL);

    r = avcodec_parameters_copy(output_stream->codecpar,
                                ctx->cover_fmt_ctx->streams[0]->codecpar);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->metadata->cover_path, NULL);
        goto end;
    }

    output_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;

    r = RESULT_OK;
end:
    return r;
}

static int maw_av_filter_crop_cover(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    AVFilterContext *filter_crop_ctx = NULL;
    const AVFilter *crop_filter = NULL;
    const AVFilter *buffersrc_filter = NULL;
    const AVFilter *buffersink_filter = NULL;
    char args[512];
    int x_offset;

    if (ctx->video_input_stream_index == -1) {
        // The validity should already have been checked during demux
        MAW_LOGF(MAW_ERROR, "%s: Video input stream index is unset",
                 ctx->mediafile->path);
        goto end;
    }

    ctx->filter_graph = avfilter_graph_alloc();
    if (ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to allocate filter graph context");
        goto end;
    }

    buffersrc_filter = avfilter_get_by_name("buffer");
    buffersink_filter = avfilter_get_by_name("buffersink");
    crop_filter = avfilter_get_by_name("crop");

    if (buffersrc_filter == NULL || buffersink_filter == NULL ||
        crop_filter == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to initialize crop filter");
        goto end;
    }

    // Input filter source: the decoded frames from the decoder will be inserted
    // here.
    r = snprintf(
        args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        ctx->dec_codec_ctx->width, ctx->dec_codec_ctx->height,
        ctx->dec_codec_ctx->pix_fmt, VIDEO_INPUT_STREAM(ctx)->time_base.num,
        VIDEO_INPUT_STREAM(ctx)->time_base.den,
        ctx->dec_codec_ctx->sample_aspect_ratio.num,
        ctx->dec_codec_ctx->sample_aspect_ratio.den);

    if (r < 0 || r >= (int)sizeof(args)) {
        MAW_LOG(MAW_ERROR, "snprintf error/truncation");
        goto end;
    }
    MAW_CREATE_FILTER(r, ctx->mediafile->path, &(ctx->filter_buffersrc_ctx),
                      buffersrc_filter, "in", ctx->filter_graph, args);

    // Crop filter: frames are cropped at this stage
    x_offset = (CROP_ACCEPTED_WIDTH - CROP_DESIRED_WIDTH) / 2;
    r = snprintf(args, sizeof(args), "w=%d:h=%d:x=%d:y=0", CROP_DESIRED_WIDTH,
                 CROP_DESIRED_HEIGHT, x_offset);

    if (r < 0 || r >= (int)sizeof(args)) {
        MAW_LOG(MAW_ERROR, "snprintf error/truncation");
        goto end;
    }
    MAW_CREATE_FILTER(r, ctx->mediafile->path, &filter_crop_ctx, crop_filter,
                      "crop", ctx->filter_graph, args);

    // Output filter sink
    MAW_CREATE_FILTER(r, ctx->mediafile->path, &(ctx->filter_buffersink_ctx),
                      buffersink_filter, "out", ctx->filter_graph,
                      (char *)NULL);

    r = avfilter_link(ctx->filter_buffersrc_ctx, 0, filter_crop_ctx, 0);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to link filters");
        goto end;
    }
    r = avfilter_link(filter_crop_ctx, 0, ctx->filter_buffersink_ctx, 0);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to link filters");
        goto end;
    }

    r = avfilter_graph_config(ctx->filter_graph, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to configure filter graph");
        goto end;
    }

    // Update relevant values in the output format
    VIDEO_OUTPUT_STREAM(ctx)->codecpar->width = CROP_DESIRED_WIDTH;
    VIDEO_OUTPUT_STREAM(ctx)->codecpar->height = CROP_DESIRED_HEIGHT;
    VIDEO_OUTPUT_STREAM(ctx)->disposition = AV_DISPOSITION_ATTACHED_PIC;

    r = RESULT_OK;
end:
    return r;
}

static int maw_av_copy_metadata_fields(AVFormatContext *fmt_ctx,
                                       const MediaFile *mediafile) {
    int r = RESULT_ERR_INTERNAL;
    char title[MAW_PATH_MAX];

    // Use the basename of the mediafile as the title if none was provided
    if (mediafile->metadata->title != NULL) {
        r = av_dict_set(&fmt_ctx->metadata, "title", mediafile->metadata->title,
                        0);
        if (r != 0)
            goto end;
    }
    else {
        r = basename_no_ext(mediafile->path, title, sizeof title);
        if (r != 0)
            goto end;
        r = av_dict_set(&fmt_ctx->metadata, "title", title, 0);
        if (r != 0)
            goto end;
    }

    r = av_dict_set(&fmt_ctx->metadata, "artist", mediafile->metadata->artist,
                    0);
    if (r != 0)
        goto end;

    r = av_dict_set(&fmt_ctx->metadata, "album", mediafile->metadata->album, 0);
    if (r != 0)
        goto end;

    r = RESULT_OK;
end:
    return r;
}

// Returns `RESULT_NOOP` if the media file already has the desired metadata set.
static int maw_av_metadata_check(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    char title[MAW_PATH_MAX];
    bool already_configured = true;
    const AVDictionaryEntry *entry = NULL;

    while (already_configured &&
           (entry = av_dict_iterate(ctx->input_fmt_ctx->metadata, entry))) {
        if (STR_EQ("title", entry->key)) {
            // No configured title -> should match filename
            if (ctx->mediafile->metadata->title == NULL) {
                r = basename_no_ext(ctx->mediafile->path, title, sizeof title);
                if (r != 0)
                    goto end;

                already_configured = STR_EQ(title, entry->value);
            }
            else {
                already_configured =
                    STR_EQ(ctx->mediafile->metadata->title, entry->value);
            }
        }
        else if (STR_EQ("artist", entry->key)) {
            if (!LHS_EMPTY_OR_EQ(ctx->mediafile->metadata->artist,
                                 entry->value)) {
                already_configured = false;
            }
        }
        else if (STR_EQ("album", entry->key)) {
            if (!LHS_EMPTY_OR_EQ(ctx->mediafile->metadata->album,
                                 entry->value)) {
                already_configured = false;
            }
        }
        else if (ctx->mediafile->metadata->clean_policy == CLEAN_POLICY_TRUE &&
                 strcmp(entry->key, "major_brand") != 0 &&
                 strcmp(entry->key, "minor_version") != 0 &&
                 strcmp(entry->key, "compatible_brands") != 0 &&
                 strcmp(entry->key, "encoder") != 0) {
            already_configured = false;
        }
    }

    if (already_configured) {
        r = RESULT_NOOP;
        goto end;
    }

    r = RESULT_OK;
end:
    return r;
}

static int maw_av_cover_check_crop(MawAVContext *ctx) {
    if (ctx->dec_codec_ctx->width == CROP_DESIRED_WIDTH &&
        ctx->dec_codec_ctx->height == CROP_DESIRED_HEIGHT) {
        MAW_LOGF(MAW_DEBUG, "%s: Crop filter has already been applied",
                 ctx->mediafile->path);
        return RESULT_NOOP;
    }
    else if (ctx->dec_codec_ctx->width != CROP_ACCEPTED_WIDTH ||
             ctx->dec_codec_ctx->height != CROP_ACCEPTED_HEIGHT) {
        MAW_LOGF(MAW_WARN,
                 "%s: Crop filter not applied: unsupported cover "
                 "dimensions: %dx%d",
                 ctx->mediafile->path, ctx->dec_codec_ctx->width,
                 ctx->dec_codec_ctx->height);
        return RESULT_NOOP;
    }

    return RESULT_OK;
}

// Returns `RESULT_NOOP` if the media file already has the desired cover.
static int maw_av_cover_check(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    char *cover_data = NULL;
    AVStream *stream = NULL;
    size_t read_bytes;

    switch (ctx->mediafile->metadata->cover_policy) {
    case COVER_POLICY_PATH:
        read_bytes =
            readfile(ctx->mediafile->metadata->cover_path, &cover_data);
        if (read_bytes == 0) {
            goto end;
        }

        if (ctx->input_fmt_ctx->nb_streams != 2) {
            MAW_LOGF(MAW_DEBUG, "%s: No pre-existing video stream",
                     ctx->mediafile->path);
            break;
        }

        stream = ctx->input_fmt_ctx->streams[1];
        if (stream->attached_pic.data == NULL) {
            MAW_LOGF(MAW_DEBUG, "%s: Video stream is empty",
                     ctx->mediafile->path);
            break;
        }
        if (stream->attached_pic.size != (int)read_bytes) {
            MAW_LOGF(MAW_DEBUG, "%s: Incorrect cover size: %d != %zu",
                     ctx->mediafile->path, stream->attached_pic.size,
                     read_bytes);
            break;
        }

        r = memcmp(stream->attached_pic.data, cover_data, read_bytes);
        if (r == 0) {
            MAW_LOGF(MAW_DEBUG, "%s: Video stream already configured",
                     ctx->mediafile->metadata->cover_path);
            r = RESULT_NOOP;
            goto end;
        }
        else {
            MAW_LOGF(MAW_DEBUG, "%s: Video stream does not match '%s'",
                     ctx->mediafile->path,
                     ctx->mediafile->metadata->cover_path);
        }
        break;
    case COVER_POLICY_CROP:
        r = maw_av_cover_check_crop(ctx);
        goto end;
    case COVER_POLICY_CLEAR:
        if (ctx->input_fmt_ctx->nb_streams == 1) {
            MAW_LOGF(MAW_DEBUG, "%s: Video stream(s) already removed",
                     ctx->mediafile->path);
            r = RESULT_NOOP;
            goto end;
        }
        break;
    case COVER_POLICY_UNSPECIFIED:
    case COVER_POLICY_KEEP:
        // Nothing to do for cover stream
        r = RESULT_NOOP;
        goto end;
    }

    r = RESULT_OK;
end:
    free(cover_data);
    return r;
}

static int maw_av_set_metadata(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    const AVDictionaryEntry *entry = NULL;

    if (ctx->mediafile->metadata->clean_policy == CLEAN_POLICY_TRUE) {
        // Only keep some of the metadata
        while ((entry = av_dict_iterate(ctx->input_fmt_ctx->metadata, entry))) {
            if (!STR_EQ("title", entry->key) && !STR_EQ("artist", entry->key) &&
                !STR_EQ("album", entry->key)) {
                continue;
            }
            r = av_dict_set(&(ctx->output_fmt_ctx->metadata), entry->key,
                            entry->value, 0);
            if (r != 0) {
                goto end;
            }
        }
    }
    else {
        // Keep the metadata as is
        r = av_dict_copy(&(ctx->output_fmt_ctx->metadata),
                         ctx->input_fmt_ctx->metadata, 0);
        if (r != 0) {
            goto end;
        }
    }

    // Set custom values
    r = maw_av_copy_metadata_fields(ctx->output_fmt_ctx, ctx->mediafile);
    if (r != 0) {
        goto end;
    }

    r = RESULT_OK;
end:
    return r;
}

// Video streams will only be demuxed if they are needed by the current policy
static int maw_av_demux(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    enum AVMediaType codec_type;
    bool is_attached_pic;
    bool metadata_already_configured;

    // Always add the audio stream first, i.e. output stream 0 will always be
    // the audio stream!
    for (ssize_t i = 0; i < ctx->input_fmt_ctx->nb_streams; i++) {
        codec_type = ctx->input_fmt_ctx->streams[i]->codecpar->codec_type;
        if (codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        if (ctx->audio_input_stream_index != -1) {
            MAW_LOGF(MAW_WARN, "%s: Audio input stream #%ld (ignored)",
                     ctx->mediafile->path, i);
            continue;
        }

        ctx->audio_input_stream_index = i;

        // Create ONE output stream for audio
        output_stream = avformat_new_stream(ctx->output_fmt_ctx, NULL);
        input_stream = ctx->input_fmt_ctx->streams[i];

        // Stream copy from the input stream onto the output
        r = avcodec_parameters_copy(output_stream->codecpar,
                                    input_stream->codecpar);

        if (r != 0) {
            MAW_AVERROR(r, ctx->mediafile->path, NULL);
            goto end;
        }
    }

    if (ctx->audio_input_stream_index == -1) {
        r = RESULT_UNSUPPORTED_INPUT_STREAMS;
        MAW_LOGF(MAW_ERROR, "%s: No audio streams", ctx->mediafile->path);
        goto end;
    }

    for (ssize_t i = 0; i < ctx->input_fmt_ctx->nb_streams; i++) {
        if (ctx->mediafile->metadata->cover_policy == COVER_POLICY_CLEAR) {
            MAW_LOGF(MAW_DEBUG, "%s: Skipping %s input stream #%ld",
                     ctx->mediafile->path, av_get_media_type_string(codec_type),
                     i);
            continue;
        }
        input_stream = ctx->input_fmt_ctx->streams[i];
        codec_type = input_stream->codecpar->codec_type;
        is_attached_pic =
            codec_type == AVMEDIA_TYPE_VIDEO &&
            input_stream->disposition == AV_DISPOSITION_ATTACHED_PIC;
        // Skip all streams except video streams with an attached_pic
        // disposition
        if (!is_attached_pic || ctx->video_input_stream_index != -1) {
            if ((int)i != ctx->audio_input_stream_index)
                MAW_LOGF(MAW_WARN, "%s: Skipping %s input stream #%ld",
                         ctx->mediafile->path,
                         av_get_media_type_string(codec_type), i);
            continue;
        }

        ctx->video_input_stream_index = i;

        output_stream = avformat_new_stream(ctx->output_fmt_ctx, NULL);
        r = avcodec_parameters_copy(output_stream->codecpar,
                                    input_stream->codecpar);
        if (r != 0) {
            MAW_AVERROR(r, ctx->mediafile->path, NULL);
            goto end;
        }

        // Set matching disposition on output stream
        // We get a deprecation warning if we do not do this since video
        // streams with an attached_pic do not have timestamps
        //  'Timestamps are unset in a packet for stream...'
        output_stream->disposition = input_stream->disposition;
    }

    r = maw_av_metadata_check(ctx);
    if (r != RESULT_OK && r != RESULT_NOOP)
        goto end;
    metadata_already_configured = r == RESULT_NOOP;

    MAW_LOGF(MAW_DEBUG, "%s: Audio input stream #%ld", ctx->mediafile->path,
             ctx->audio_input_stream_index);

    if (ctx->video_input_stream_index != -1) {
        MAW_LOGF(MAW_DEBUG, "%s: Video input stream #%ld", ctx->mediafile->path,
                 ctx->video_input_stream_index);

        // Initialize decoder context for cropping
        if (ctx->mediafile->metadata->cover_policy == COVER_POLICY_CROP) {
            r = maw_av_init_dec_context(ctx);
            if (r != 0)
                goto end;
        }

        // Return NOOP if the video streams are already configured.
        if (metadata_already_configured &&
            ctx->input_fmt_ctx->nb_streams == 2) {
            r = maw_av_cover_check(ctx);
            if (r == RESULT_NOOP) {
                // OK: there is no need to remux this file
                goto end;
            }
            else if (r != RESULT_OK) {
                goto end;
            }
        }
    }
    else {
        MAW_LOGF(MAW_DEBUG, "%s: Video input stream (none)",
                 ctx->mediafile->path);

        // Return NOOP if we have the expected number of streams and
        // do not need to change the metadata or cover.
        if (metadata_already_configured &&
            ctx->input_fmt_ctx->nb_streams == 1 &&
            ctx->mediafile->metadata->cover_policy != COVER_POLICY_PATH) {
            r = RESULT_NOOP;
            goto end;
        }
    }

    r = RESULT_OK;
end:
    return r;
}

static int maw_av_mux_crop(MawAVContext *ctx, AVPacket *pkt) {
    int r = RESULT_ERR_INTERNAL;
    AVFrame *filtered_frame = NULL;
    AVFrame *frame = NULL;

    // Create an encoder context to translate the output frames from the
    // filtergraph back into packets
    r = maw_av_init_enc_context(ctx);
    if (r != 0)
        goto end;

    frame = av_frame_alloc();
    filtered_frame = av_frame_alloc();
    if (frame == NULL || filtered_frame == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to initialize filter structures");
        goto end;
    }

    // Send the packet to the decoder
    r = avcodec_send_packet(ctx->dec_codec_ctx, pkt);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to send packet to decoder");
        goto end;
    }
    av_packet_unref(pkt);

    // Read the decoded frame (XXX: EAGAIN is not handled)
    r = avcodec_receive_frame(ctx->dec_codec_ctx, frame);
    if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
        goto end;
    }
    else if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to read decoded frame");
        goto end;
    }

    // Push the frame into the filter graph
    r = av_buffersrc_add_frame(ctx->filter_buffersrc_ctx, frame);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Error feeding the filtergraph");
        goto end;
    }
    av_frame_free(&frame);

    // Pull filtered frames from the filtergraph
    r = av_buffersink_get_frame(ctx->filter_buffersink_ctx, filtered_frame);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to read filtered frame");
        goto end;
    }

    // Encode the frame into a packet
    r = avcodec_send_frame(ctx->enc_codec_ctx, filtered_frame);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Error sending frame to encoder");
        goto end;
    }
    av_frame_free(&filtered_frame);

    // Read back the encoded packet
    av_packet_unref(pkt);
    r = avcodec_receive_packet(ctx->enc_codec_ctx, pkt);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to read filtered packet");
        goto end;
    }

    // Write the encoded packet to the output stream
    pkt->pos = -1;
    pkt->pts = AV_NOPTS_VALUE;
    pkt->stream_index = VIDEO_OUTPUT_STREAM_INDEX;

    // Verify that there are not more filtered frames to process
    r = av_buffersink_get_frame(ctx->filter_buffersink_ctx, filtered_frame);
    if (r != AVERROR(EAGAIN)) {
        MAW_LOG(MAW_ERROR, "Did not read all frames from stream");
        goto end;
    }

    r = RESULT_OK;
end:
    av_frame_free(&frame);
    av_frame_free(&filtered_frame);
    return r;
}

static int maw_av_mux(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    int prev_stream_index = -1;
    int output_stream_index = -1;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    // Packets contain compressed data from a stream, frames contain the
    // actual raw data. Filters can not be applied directly on packets, we
    // need to decode them into frames and re-encode them back into packets.
    AVPacket *pkt = NULL;
    bool should_crop =
        ctx->mediafile->metadata->cover_policy == COVER_POLICY_CROP &&
        ctx->video_input_stream_index != -1 &&
        ctx->dec_codec_ctx->width == CROP_ACCEPTED_WIDTH &&
        ctx->dec_codec_ctx->height == CROP_ACCEPTED_HEIGHT;

    r = avio_open(&(ctx->output_fmt_ctx->pb), ctx->output_filepath,
                  AVIO_FLAG_WRITE);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, NULL);
        goto end;
    }

    r = avformat_write_header(ctx->output_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to write header");
        goto end;
    }

    pkt = av_packet_alloc();
    if (pkt == NULL) {
        MAW_LOGF(MAW_ERROR, "%s: Failed to allocate packet",
                 ctx->mediafile->path);
        goto end;
    }

    // Mux streams from input file
    while (av_read_frame(ctx->input_fmt_ctx, pkt) == 0) {
        if (pkt->stream_index < 0 ||
            pkt->stream_index >= (int)ctx->input_fmt_ctx->nb_streams) {
            MAW_LOGF(MAW_ERROR, "%s: Invalid stream index: #%d",
                     ctx->mediafile->path, pkt->stream_index);
            goto end;
        }

        input_stream = ctx->input_fmt_ctx->streams[pkt->stream_index];

        if (pkt->stream_index == ctx->audio_input_stream_index) {
            // Audio stream
            output_stream_index = AUDIO_OUTPUT_STREAM_INDEX;
        }
        else if (pkt->stream_index == ctx->video_input_stream_index) {
            if (ctx->mediafile->metadata->cover_policy == COVER_POLICY_PATH) {
                // Skip original video stream
                av_packet_unref(pkt);
                continue;
            }
            // Video stream to keep
            output_stream_index = VIDEO_OUTPUT_STREAM_INDEX;
        }
        else {
            // Rate limit repeated log messages
            if (prev_stream_index != pkt->stream_index)
                MAW_LOGF(MAW_DEBUG,
                         "%s: Ignoring packets from %s input stream #%d",
                         ctx->mediafile->path,
                         av_get_media_type_string(
                             input_stream->codecpar->codec_type),
                         pkt->stream_index);
            prev_stream_index = pkt->stream_index;
            av_packet_unref(pkt);
            continue;
        }

        output_stream = ctx->output_fmt_ctx->streams[output_stream_index];

        // The `pkt->stream_index` is set to the stream index in the input file.
        // Remap it to the correct `stream_index` in the output file.
        pkt->stream_index = output_stream_index;

        if (should_crop && pkt->stream_index == ctx->video_input_stream_index) {
            r = maw_av_mux_crop(ctx, pkt);
            if (r != 0)
                goto end;
        }

        if (pkt->stream_index == ctx->audio_input_stream_index) {
            av_packet_rescale_ts(pkt, input_stream->time_base,
                                 output_stream->time_base);
            pkt->pos = -1;
        }
        // The pkt passed to this function is automatically freed
        r = av_interleaved_write_frame(ctx->output_fmt_ctx, pkt);
        if (r != 0) {
            MAW_AVERROR(r, ctx->mediafile->path, "Failed to mux packet");
            goto end;
        }
        // This warning: 'Encoder did not produce proper pts, making some up.'
        // appears for packets in cover art streams (since they do not have a
        // pts value set), its harmless, more info on pts:
        // http://dranger.com/ffmpeg/tutorial05.html
    }

    // Mux streams from cover
    while (ctx->cover_fmt_ctx != NULL) {
        r = av_read_frame(ctx->cover_fmt_ctx, pkt);
        if (r != 0) {
            break; // No more frames
        }

        if (pkt->stream_index != 0) {
            r = RESULT_ERR_INTERNAL;
            MAW_LOGF(MAW_ERROR, "Unexpected packet from cover stream #%d",
                     pkt->stream_index);
            goto end;
        }

        pkt->stream_index = VIDEO_OUTPUT_STREAM_INDEX;
        pkt->pos = -1;

        r = av_interleaved_write_frame(ctx->output_fmt_ctx, pkt);
        if (r != 0) {
            MAW_AVERROR(r, ctx->mediafile->path, "Failed to mux packet");
            break;
        }
    }

    r = av_write_trailer(ctx->output_fmt_ctx);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to write trailer");
        goto end;
    }

    r = RESULT_OK;
end:
    av_packet_free(&pkt);
    return r;
}

static int maw_av_init_dec_context(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    const AVCodec *dec_codec = NULL;

    dec_codec =
        avcodec_find_decoder(VIDEO_INPUT_STREAM(ctx)->codecpar->codec_id);
    if (dec_codec == NULL) {
        MAW_LOG(MAW_ERROR, "Failed to find decoder");
        goto end;
    }

    ctx->dec_codec_ctx = avcodec_alloc_context3(NULL);
    if (ctx->dec_codec_ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to allocate decoder context");
        goto end;
    }

    r = avcodec_parameters_to_context(ctx->dec_codec_ctx,
                                      VIDEO_INPUT_STREAM(ctx)->codecpar);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to copy codec parameters");
        goto end;
    }

    r = avcodec_open2(ctx->dec_codec_ctx, dec_codec, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to open decoder context");
        goto end;
    }

    MAW_LOGF(
        MAW_DEBUG,
        "%s: Video stream #%ld: video_size=%dx%d pix_fmt=%s pixel_aspect=%d/%d",
        ctx->mediafile->path, ctx->video_input_stream_index,
        ctx->dec_codec_ctx->width, ctx->dec_codec_ctx->height,
        av_get_pix_fmt_name(ctx->dec_codec_ctx->pix_fmt),
        ctx->dec_codec_ctx->sample_aspect_ratio.num,
        ctx->dec_codec_ctx->sample_aspect_ratio.den);

    r = RESULT_OK;
end:
    return r;
}

static int maw_av_init_enc_context(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;
    const AVCodec *enc_codec = NULL;

    enc_codec =
        avcodec_find_encoder(VIDEO_INPUT_STREAM(ctx)->codecpar->codec_id);
    if (enc_codec == NULL) {
        MAW_LOG(MAW_ERROR, "Failed to find encoder");
        goto end;
    }

    ctx->enc_codec_ctx = avcodec_alloc_context3(enc_codec);
    if (ctx->enc_codec_ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, ctx->mediafile->path,
                    "Failed to allocate encoder context");
        goto end;
    }

    ctx->enc_codec_ctx->time_base = (AVRational){1, 1};
    ctx->enc_codec_ctx->framerate =
        VIDEO_INPUT_STREAM(ctx)->codecpar->framerate;
    ctx->enc_codec_ctx->max_b_frames = 1;
    // Use the same dimensions as the output stream
    ctx->enc_codec_ctx->width = CROP_DESIRED_WIDTH;
    ctx->enc_codec_ctx->height = CROP_DESIRED_HEIGHT;
    // Use the same pix_fmt as the decoder
    ctx->enc_codec_ctx->pix_fmt = ctx->dec_codec_ctx->pix_fmt;

    r = avcodec_open2(ctx->enc_codec_ctx, enc_codec, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to open encoder context");
        goto end;
    }

    r = RESULT_OK;
end:
    return r;
}

// The remux process only applies a filter when COVER_POLICY_CROP is set,
// otherwise a "Stream copy", see ffmpeg(1), is performed.
int maw_av_remux(MawAVContext *ctx) {
    int r = RESULT_ERR_INTERNAL;

    // Find the indices of the video and audio stream and create
    // corresponding output streams.
    r = maw_av_demux(ctx);
    if (r == RESULT_NOOP) {
        // Current metadata configuration has already been applied
        goto end;
    }
    else if (r != RESULT_OK) {
        goto end;
    }

    // Only try to crop if there is a valid input video stream...
    if (ctx->mediafile->metadata->cover_policy == COVER_POLICY_CROP &&
        ctx->video_input_stream_index != -1) {
        // ... and valid input cover dimensions
        r = maw_av_cover_check_crop(ctx);

        if (r == RESULT_OK) {
            MAW_LOGF(MAW_DEBUG, "%s: Applying crop filter",
                     ctx->mediafile->path);
            // Initialize a filter to crop the existing video stream
            r = maw_av_filter_crop_cover(ctx);
            if (r != 0)
                goto end;
        }
    }
    else if (ctx->mediafile->metadata->cover_policy == COVER_POLICY_PATH) {
        // Find the input stream in the cover and create a corresponding
        // output stream
        r = maw_av_demux_picture_file(ctx);
        if (r != 0)
            goto end;
    }

    // Configure metadata
    r = maw_av_set_metadata(ctx);
    if (r != 0) {
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to copy metadata");
        goto end;
    }

    // Write the demuxed content back to disk (via filter if applicable)
    r = maw_av_mux(ctx);
    if (r != 0)
        goto end;

    r = RESULT_OK;
end:
    return r;
}

void maw_av_free_context(MawAVContext *ctx) {
    if (ctx == NULL)
        return;

    avformat_close_input(&ctx->input_fmt_ctx);
    if (ctx->input_fmt_ctx != NULL) {
        avformat_free_context(ctx->input_fmt_ctx);
    }

    avformat_close_input(&ctx->cover_fmt_ctx);
    if (ctx->cover_fmt_ctx != NULL) {
        avformat_free_context(ctx->cover_fmt_ctx);
    }

    if (ctx->output_fmt_ctx != NULL) {
        if (ctx->output_fmt_ctx->oformat != NULL &&
            !(ctx->output_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&(ctx->output_fmt_ctx->pb));
        }
        avformat_free_context(ctx->output_fmt_ctx);
    }

    avcodec_free_context(&ctx->enc_codec_ctx);
    avcodec_free_context(&ctx->dec_codec_ctx);
    avfilter_graph_free(&ctx->filter_graph);

    free(ctx);
}

MawAVContext *maw_av_init_context(const MediaFile *mediafile,
                                  const char *output_filepath) {
    int r;
    MawAVContext *ctx = NULL;
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;

    // Create context for input file
    r = avformat_open_input(&input_fmt_ctx, mediafile->path, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, mediafile->path, NULL);
        goto end;
    }
    // Read input file metadata
    r = avformat_find_stream_info(input_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, mediafile->path, NULL);
        goto end;
    }

    // Create a context for the output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL,
                                       output_filepath);
    if (r != 0) {
        MAW_AVERROR(r, output_filepath, NULL);
        goto end;
    }

    ctx = calloc(1, sizeof(MawAVContext));
    if (ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, ctx->mediafile->path, "Failed to allocate context");
        goto end;
    }

    ctx->input_fmt_ctx = input_fmt_ctx;
    ctx->output_fmt_ctx = output_fmt_ctx;
    ctx->audio_input_stream_index = -1;
    ctx->video_input_stream_index = -1;
    ctx->mediafile = mediafile;
    ctx->output_filepath = output_filepath;
    // Filtering variables
    ctx->filter_graph = NULL;
    ctx->filter_buffersrc_ctx = NULL;
    ctx->filter_buffersink_ctx = NULL;
    ctx->dec_codec_ctx = NULL;
    ctx->enc_codec_ctx = NULL;
end:
    return ctx;
}

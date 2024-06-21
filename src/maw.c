#include "maw.h"
#include "libavutil/rational.h"
#include "log.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/avassert.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>

#include <unistd.h>

static int maw_demux_cover(MawContext*);
static int maw_filter_crop_cover(MawContext*);
static int maw_copy_metadata_fields(AVFormatContext*, const Metadata*);
static int maw_set_metadata(MawContext*);
static int maw_demux(MawContext*);
static int maw_mux(MawContext*);
static int maw_init_dec_context(MawContext*);
static int maw_init_enc_context(MawContext*);
static int maw_remux(MawContext*);
static void maw_free_context(MawContext*);
static MawContext* maw_init_context(const Metadata*, const char*);

////////////////////////////////////////////////////////////////////////////////

static int maw_demux_cover(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    AVStream *output_stream = NULL;
    enum AVMediaType codec_type;

    // Demux the input file
    r = avformat_open_input(&(ctx->cover_fmt_ctx), ctx->metadata->cover_path, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->metadata->cover_path);
        goto end;
    }
    r = avformat_find_stream_info(ctx->cover_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, ctx->metadata->cover_path);
        goto end;
    }

    if (ctx->cover_fmt_ctx->nb_streams == 0) {
        MAW_LOGF(MAW_ERROR, "%s: cover has no input streams", ctx->metadata->cover_path);
        r = UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }
    if (ctx->cover_fmt_ctx->nb_streams > 1) {
        MAW_LOGF(MAW_ERROR, "%s: cover has more than one input stream", ctx->metadata->cover_path);
        r = UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }
    codec_type = ctx->cover_fmt_ctx->streams[0]->codecpar->codec_type;
    if (codec_type != AVMEDIA_TYPE_VIDEO) {
        MAW_LOGF(MAW_ERROR, "%s: cover does not contain a video stream (found %s)",
                 ctx->metadata->cover_path,
                 av_get_media_type_string(codec_type));
        r = UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }

    output_stream = avformat_new_stream(ctx->output_fmt_ctx, NULL);

    r = avcodec_parameters_copy(output_stream->codecpar,
                                ctx->cover_fmt_ctx->streams[0]->codecpar);
    if (r != 0) {
        MAW_AVERROR(r, ctx->metadata->cover_path);
        goto end;
    }

    output_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;

    r = 0;
end:
    return r;
}

static int maw_filter_crop_cover(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    AVFilterContext *filter_crop_ctx = NULL;
    const AVFilter *crop_filter = NULL;
    const AVFilter *buffersrc_filter  = NULL;
    const AVFilter *buffersink_filter = NULL;
    char args[512];
    int x_offset;

    if (ctx->video_input_stream_index == -1) {
        // The validity should already have been checked during demux
        goto end;
    }

    ctx->filter_graph = avfilter_graph_alloc();
    if (ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Failed to allocate filter graph context");
        goto end;
    }

    buffersrc_filter  = avfilter_get_by_name("buffer");
    buffersink_filter = avfilter_get_by_name("buffersink");
    crop_filter = avfilter_get_by_name("crop");

    if (buffersrc_filter == NULL ||
        buffersink_filter == NULL ||
        crop_filter == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Failed to initialize crop filter");
        goto end;
    }

    // Input filter source: the decoded frames from the decoder will be inserted here.
    r = snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 ctx->dec_codec_ctx->width, ctx->dec_codec_ctx->height, ctx->dec_codec_ctx->pix_fmt,
                 VIDEO_INPUT_STREAM(ctx)->time_base.num, VIDEO_INPUT_STREAM(ctx)->time_base.den,
                 ctx->dec_codec_ctx->sample_aspect_ratio.num, ctx->dec_codec_ctx->sample_aspect_ratio.den);

    if (r < 0 || r >= (int)sizeof(args)) {
        MAW_LOG(MAW_ERROR, "snprintf error/truncation");
        goto end;
    }
    MAW_CREATE_FILTER(r, &(ctx->filter_buffersrc_ctx), buffersrc_filter, "in", ctx->filter_graph, args);

    // Crop filter: frames are cropped at this stage
    x_offset = (CROP_ACCEPTED_WIDTH - CROP_DESIRED_WIDTH) / 2;
    r = snprintf(args, sizeof(args), "w=%d:h=%d:x=%d:y=0",
                 CROP_DESIRED_WIDTH, CROP_DESIRED_HEIGHT, x_offset);

    if (r < 0 || r >= (int)sizeof(args)) {
        MAW_LOG(MAW_ERROR, "snprintf error/truncation");
        goto end;
    }
    MAW_CREATE_FILTER(r, &filter_crop_ctx, crop_filter, "crop", ctx->filter_graph, args);

    // Output filter sink
    MAW_CREATE_FILTER(r, &(ctx->filter_buffersink_ctx), buffersink_filter, "out", ctx->filter_graph, (char*)NULL);

    r = avfilter_link(ctx->filter_buffersrc_ctx, 0, filter_crop_ctx, 0);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to link filters");
        goto end;
    }
    r = avfilter_link(filter_crop_ctx, 0, ctx->filter_buffersink_ctx, 0);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to link filters");
        goto end;
    }

    r = avfilter_graph_config(ctx->filter_graph, NULL);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to configure filter graph");
        goto end;
    }

    // Update relevant values in the output format
    VIDEO_OUTPUT_STREAM(ctx)->codecpar->width = CROP_DESIRED_WIDTH;
    VIDEO_OUTPUT_STREAM(ctx)->codecpar->height = CROP_DESIRED_HEIGHT;
    VIDEO_OUTPUT_STREAM(ctx)->disposition = AV_DISPOSITION_ATTACHED_PIC;

    r = 0;
end:
    return r;
}

static int maw_copy_metadata_fields(AVFormatContext *fmt_ctx,
                                    const Metadata *metadata) {
    int r = INTERNAL_ERROR;
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

// @return 0 on success, negative AVERROR code on failure.
static int maw_set_metadata(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    const AVDictionaryEntry *entry = NULL;

    if (ctx->metadata->clear_non_core_fields) {
        // Only keep some of the metadata
        while ((entry = av_dict_iterate(ctx->input_fmt_ctx->metadata, entry))) {
            if (strcmp(entry->key, "title") != 0 &&
                strcmp(entry->key, "artist") != 0 &&
                strcmp(entry->key, "album") != 0) {
                continue;
            }
            r = av_dict_set(&(ctx->output_fmt_ctx->metadata), entry->key, entry->value, 0);
            if (r != 0) {
                goto end;
            }
        }
    } else {
        // Keep the metadata as is
        r = av_dict_copy(&(ctx->output_fmt_ctx->metadata), ctx->input_fmt_ctx->metadata, 0);
        if (r != 0) {
            goto end;
        }
    }

    // Set custom values
    if (ctx->metadata != NULL) {
        r = maw_copy_metadata_fields(ctx->output_fmt_ctx, ctx->metadata);
        if (r != 0) {
            goto end;
        }
    }

    r = 0;
end:
    return r;
}

// Video streams will only be demuxed if they are needed by the current policy
static int maw_demux(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    enum AVMediaType codec_type;
    bool is_attached_pic;

    // Always add the audio stream first, i.e. output stream 0 will always be the
    // audio stream!
    for (unsigned int i = 0; i < ctx->input_fmt_ctx->nb_streams; i++) {
        codec_type = ctx->input_fmt_ctx->streams[i]->codecpar->codec_type;
        if (codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        if (ctx->audio_input_stream_index != -1) {
            MAW_LOGF(MAW_WARN, "%s: Audio input stream #%u (ignored)", ctx->metadata->filepath, i);
            continue;
        }

        ctx->audio_input_stream_index = i;

        // Create ONE output stream for audio
        output_stream = avformat_new_stream(ctx->output_fmt_ctx, NULL);
        input_stream = ctx->input_fmt_ctx->streams[i];

        // Stream copy from the input stream onto the output
        r = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);

        if (r != 0) {
            MAW_AVERROR(r, ctx->output_filepath);
            goto end;
        }
    }

    if (ctx->audio_input_stream_index == -1) {
        r = UNSUPPORTED_INPUT_STREAMS;
        MAW_LOGF(MAW_ERROR, "%s: No audio streams", ctx->metadata->filepath);
        goto end;
    }

    for (unsigned int i = 0; i < ctx->input_fmt_ctx->nb_streams; i++) {
        input_stream = ctx->input_fmt_ctx->streams[i];
        codec_type = input_stream->codecpar->codec_type;
        is_attached_pic = codec_type == AVMEDIA_TYPE_VIDEO &&
                          input_stream->disposition == AV_DISPOSITION_ATTACHED_PIC;
        // Skip all streams except video streams with an attached_pic disposition
        if (!is_attached_pic || ctx->video_input_stream_index != -1) {
            if ((int)i != ctx->audio_input_stream_index)
                MAW_LOGF(MAW_WARN, "%s: Skipping %s input stream #%d",
                                    ctx->metadata->filepath, av_get_media_type_string(codec_type), i);
            continue;
        }

        ctx->video_input_stream_index = i;

        // Do not demux the original video stream if it is not needed
        if (!NEEDS_ORIGINAL_COVER(ctx->metadata)) {
            continue;
        }

        output_stream = avformat_new_stream(ctx->output_fmt_ctx, NULL);
        r = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);
        if (r != 0) {
            MAW_AVERROR(r, ctx->output_filepath);
            goto end;
        }

        // Set matching disposition on output stream
        // We get a deprecation warning if we do not do this since video
        // streams with an attached_pic do not have timestamps
        //  'Timestamps are unset in a packet for stream...'
        output_stream->disposition = input_stream->disposition;
    }

    MAW_LOGF(MAW_DEBUG, "%s: Audio input stream #%d", ctx->metadata->filepath,
                                                        ctx->audio_input_stream_index);

    if (ctx->video_input_stream_index != -1) {
        if (NEEDS_ORIGINAL_COVER(ctx->metadata)) {
            MAW_LOGF(MAW_DEBUG, "%s: Video input stream #%d", ctx->metadata->filepath,
                                                                ctx->video_input_stream_index);
        }
        else {
            MAW_LOGF(MAW_DEBUG, "%s: Video input stream #%d (ignored)", ctx->metadata->filepath,
                                                                          ctx->video_input_stream_index);
        }
    }
    else {
        MAW_LOGF(MAW_DEBUG, "%s: Video input stream (none)", ctx->metadata->filepath);
    }

    r = 0;
end:
    return r;
}

static int maw_mux(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    int prev_stream_index = -1;
    int output_stream_index = -1;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    // Packets contain compressed data from a stream, frames contain the
    // actual raw data. Filters can not be applied directly on packets, we
    // need to decode them into frames and re-encode them back into packets.
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    AVFrame *filtered_frame = NULL;
    bool should_crop = ctx->metadata->cover_policy == CROP_COVER &&
                       ctx->dec_codec_ctx->width == CROP_ACCEPTED_WIDTH &&
                       ctx->dec_codec_ctx->height == CROP_ACCEPTED_HEIGHT;

    r = avio_open(&(ctx->output_fmt_ctx->pb), ctx->output_filepath, AVIO_FLAG_WRITE);
    if (r != 0) {
        MAW_AVERROR(r, ctx->output_filepath);
        goto end;
    }

    r = avformat_write_header(ctx->output_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to write header");
        goto end;
    }

    pkt = av_packet_alloc();
    if (pkt == NULL) {
        MAW_LOGF(MAW_ERROR, "%s: Failed to allocate packet", ctx->output_filepath);
        goto end;
    }

    if (should_crop) {
        // Create an encoder context, we need this to translate the output frames
        // from the filtergraph back into packets
        r = maw_init_enc_context(ctx);
        if (r != 0)
            goto end;

        frame = av_frame_alloc();
        filtered_frame = av_frame_alloc();
        if (frame == NULL || filtered_frame == NULL) {
            r = AVERROR(ENOMEM);
            MAW_AVERROR(r, "Failed to initialize filter structures");
            goto end;
        }
    }

    // Mux streams from input file
    while (av_read_frame(ctx->input_fmt_ctx, pkt) == 0) {
        if (pkt->stream_index < 0 ||
            pkt->stream_index >= (int)ctx->input_fmt_ctx->nb_streams) {
            MAW_LOGF(MAW_ERROR, "%s: Invalid stream index: #%d", ctx->metadata->filepath,
                                                                   pkt->stream_index);
            goto end;
        }

        input_stream = ctx->input_fmt_ctx->streams[pkt->stream_index];

        if (pkt->stream_index == ctx->audio_input_stream_index) {
            // Audio stream
            output_stream_index = AUDIO_OUTPUT_STREAM_INDEX;
        }
        else if (pkt->stream_index == ctx->video_input_stream_index) {
            if (!NEEDS_ORIGINAL_COVER(ctx->metadata)) {
                // Skip original video stream
                continue;
            }
            // Video stream to keep
            output_stream_index = VIDEO_OUTPUT_STREAM_INDEX;
        }
        else {
            // Rate limit repeated log messages
            if (prev_stream_index != pkt->stream_index)
                MAW_LOGF(MAW_DEBUG, "%s: Ignoring packets from %s input stream #%d",
                         ctx->metadata->filepath,
                         av_get_media_type_string(input_stream->codecpar->codec_type),
                         pkt->stream_index);
            prev_stream_index = pkt->stream_index;
            continue;
        }

        output_stream = ctx->output_fmt_ctx->streams[output_stream_index];

        // The pkt will have the stream_index set to the stream index in the
        // input file. Remap it to the correct stream_index in the output file.
        pkt->stream_index = output_stream_index;

        if (should_crop && pkt->stream_index == ctx->video_input_stream_index) {
            // Send the packet to the decoder
            r = avcodec_send_packet(ctx->dec_codec_ctx, pkt);
            if (r != 0) {
                MAW_AVERROR(r, "Failed to send packet to decoder");
                goto end;
            }
            // Read the decoded frame
            r = avcodec_receive_frame(ctx->dec_codec_ctx, frame);
            if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
                break;
            }
            else if (r != 0) {
                MAW_AVERROR(r, "Failed to read decoded frame");
                goto end;
            }

            // Push the frame into the filter graph
            r = av_buffersrc_add_frame_flags(ctx->filter_buffersrc_ctx,
                                             frame,
                                             AV_BUFFERSRC_FLAG_KEEP_REF);
            if (r != 0) {
                MAW_AVERROR(r, "Error feeding the filtergraph");
                goto end;
            }
            av_frame_unref(frame);

            // Pull filtered frames from the filtergraph
            while (true) {
                r = av_buffersink_get_frame(ctx->filter_buffersink_ctx, filtered_frame);
                if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
                    break;
                }
                else if (r != 0) {
                    MAW_AVERROR(r, "Failed to read filtered frame");
                    goto end;
                }

                // Encode the frame into a packet
                r = avcodec_send_frame(ctx->enc_codec_ctx, filtered_frame);
                if (r != 0) {
                    MAW_AVERROR(r, "Error sending frame to encoder");
                    goto end;
                }
                // Read back the encoded packet
                r = avcodec_receive_packet(ctx->enc_codec_ctx, pkt);
                if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
                    break;
                }
                else if (r != 0) {
                    MAW_AVERROR(r, "Failed to read filtered packet");
                    goto end;
                }

                // Write the encoded packet to the output stream
                pkt->pos = -1;
                pkt->pts = AV_NOPTS_VALUE;
                pkt->stream_index = 1;
            }
        }

        if (pkt->stream_index == ctx->audio_input_stream_index) {
            av_packet_rescale_ts(pkt, input_stream->time_base,
                                      output_stream->time_base);
            pkt->pos = -1;
        }
        // The pkt passed to this function is automatically freed
        r = av_interleaved_write_frame(ctx->output_fmt_ctx, pkt);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to mux packet");
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
            r = INTERNAL_ERROR;
            MAW_LOGF(MAW_ERROR, "Unexpected packet from cover stream #%d",
                     pkt->stream_index);
            goto end;
        }

        output_stream_index = VIDEO_OUTPUT_STREAM_INDEX;

        // Input and output stream for the current packet
        input_stream = ctx->cover_fmt_ctx->streams[pkt->stream_index];
        output_stream = ctx->output_fmt_ctx->streams[output_stream_index];

        pkt->stream_index = output_stream_index;
        pkt->pos = -1;

        r = av_interleaved_write_frame(ctx->output_fmt_ctx, pkt);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to mux packet");
            break;
        }
    }

    r = av_write_trailer(ctx->output_fmt_ctx);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to write trailer");
        goto end;
    }

end:
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&filtered_frame);
    return r;
}

static int maw_init_dec_context(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    const AVCodec *dec_codec = NULL;

    dec_codec = avcodec_find_decoder(VIDEO_INPUT_STREAM(ctx)->codecpar->codec_id);
    if (dec_codec == NULL) {
        MAW_LOG(MAW_ERROR, "Failed to find decoder");
        goto end;
    }

    ctx->dec_codec_ctx = avcodec_alloc_context3(NULL);
    if (ctx->dec_codec_ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Failed to allocate decoder context");
        goto end;
    }

    r = avcodec_parameters_to_context(ctx->dec_codec_ctx, VIDEO_INPUT_STREAM(ctx)->codecpar);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to copy codec parameters");
        goto end;
    }

    r = avcodec_open2(ctx->dec_codec_ctx, dec_codec, NULL);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to open decoder context");
        goto end;
    }

    MAW_LOGF(MAW_DEBUG, "%s: Video stream #%d: video_size=%dx%d pix_fmt=%s pixel_aspect=%d/%d",
         ctx->metadata->filepath, ctx->video_input_stream_index,
         ctx->dec_codec_ctx->width, ctx->dec_codec_ctx->height,
         av_get_pix_fmt_name(ctx->dec_codec_ctx->pix_fmt),
         ctx->dec_codec_ctx->sample_aspect_ratio.num, ctx->dec_codec_ctx->sample_aspect_ratio.den);
    r = 0;
end:
    return r;
}

static int maw_init_enc_context(MawContext *ctx) {
    int r = INTERNAL_ERROR;
    const AVCodec *enc_codec = NULL;

    enc_codec = avcodec_find_encoder(VIDEO_INPUT_STREAM(ctx)->codecpar->codec_id);
    if (enc_codec == NULL) {
        MAW_LOG(MAW_ERROR, "Failed to find encoder");
        goto end;
    }

    ctx->enc_codec_ctx = avcodec_alloc_context3(enc_codec);
    if (ctx->enc_codec_ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Failed to allocate encoder context");
        goto end;
    }

    ctx->enc_codec_ctx->time_base = (AVRational){1,1};
    ctx->enc_codec_ctx->framerate = VIDEO_INPUT_STREAM(ctx)->codecpar->framerate;
    ctx->enc_codec_ctx->max_b_frames = 1;
    // Use the same dimensions as the output stream
    ctx->enc_codec_ctx->width = CROP_DESIRED_WIDTH;
    ctx->enc_codec_ctx->height = CROP_DESIRED_HEIGHT;
    // Use the same pix_fmt as the decoder
    ctx->enc_codec_ctx->pix_fmt = ctx->dec_codec_ctx->pix_fmt;

    r = avcodec_open2(ctx->enc_codec_ctx, enc_codec, NULL);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to open encoder context");
        goto end;
    }
    r = 0;
end:
    return r;
}

// See "Stream copy" section of ffmpeg(1), that is what we are doing
static int maw_remux(MawContext *ctx) {
    int r = INTERNAL_ERROR;

    // * Find the indices of the video and audio stream and create
    // corresponding output streams
    r = maw_demux(ctx);
    if (r != 0)
        goto end;

    if (ctx->metadata->cover_path != NULL && strlen(ctx->metadata->cover_path) > 0) {
        // * Find the input stream in the cover and create a corresponding output
        // stream
        r = maw_demux_cover(ctx);
        if (r != 0)
            goto end;
    }
    else if (ctx->metadata->cover_policy == CROP_COVER) {
        r = maw_init_dec_context(ctx);
        if (r != 0)
            goto end;

        if (ctx->dec_codec_ctx->width == CROP_DESIRED_WIDTH &&
            ctx->dec_codec_ctx->height == CROP_DESIRED_HEIGHT) {
            MAW_LOGF(MAW_DEBUG, "%s: Crop filter has already been applied", ctx->metadata->filepath);
        }
        else if (ctx->dec_codec_ctx->width != CROP_ACCEPTED_WIDTH ||
                 ctx->dec_codec_ctx->height != CROP_ACCEPTED_HEIGHT) {
            MAW_LOGF(MAW_WARN, "%s: Crop filter not applied: unsupported cover dimensions: %dx%d",
                               ctx->metadata->filepath, ctx->dec_codec_ctx->width, ctx->dec_codec_ctx->height);
        }
        else {
            MAW_LOGF(MAW_DEBUG, "%s: Applying crop filter", ctx->metadata->filepath);

            // * Initialize a filter to crop the existing video stream
            r = maw_filter_crop_cover(ctx);
            if (r != 0)
                goto end;
        }
    }

    // * Configure metadata
    r = maw_set_metadata(ctx);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to copy metadata");
        goto end;
    }

    // * Write the demuxed content back to disk (via filter if applicable)
    r = maw_mux(ctx);
    if (r != 0)
        goto end;

end:
    return r;
}

static void maw_free_context(MawContext *ctx) {
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

static MawContext* maw_init_context(const Metadata *metadata,
                                    const char *output_filepath) {
    int r;
    MawContext *ctx = NULL;
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;

    // Create context for input file
    r = avformat_open_input(&input_fmt_ctx, metadata->filepath, NULL, NULL);
    if (r != 0) {
        MAW_AVERROR(r, metadata->filepath);
        goto end;
    }
    // Read input file metadata
    r = avformat_find_stream_info(input_fmt_ctx, NULL);
    if (r != 0) {
        MAW_AVERROR(r, metadata->filepath);
        goto end;
    }

    // Create a context for the output file
    // Possible formats: `ffmpeg -formats`
    r = avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filepath);
    if (r != 0) {
        MAW_AVERROR(r, output_filepath);
        goto end;
    }

    ctx = calloc(1, sizeof(MawContext));
    if (ctx == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Failed to allocate context");
        goto end;
    }

    ctx->input_fmt_ctx = input_fmt_ctx;
    ctx->output_fmt_ctx = output_fmt_ctx;
    ctx->audio_input_stream_index = -1;
    ctx->video_input_stream_index = -1;
    // XXX: Not reallocated
    ctx->metadata = metadata;
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

int maw_update(const Metadata *metadata) {
    int r = INTERNAL_ERROR;
    char tmpfile[] = "/tmp/maw.XXXXX.m4a";
    int tmphandle = mkstemps(tmpfile, sizeof(".m4a") - 1);
    MawContext *ctx = NULL;

    if (tmphandle < 0) {
         MAW_PERROR(tmpfile);
         goto end;
    }
    (void)close(tmphandle);

    MAW_LOGF(MAW_DEBUG, "%s -> %s", metadata->filepath, tmpfile);

    ctx = maw_init_context(metadata, tmpfile);
    if (ctx == NULL)
        goto end;

    r = maw_remux(ctx);
    if (r != 0) {
        goto end;
    }

    r = rename(tmpfile, metadata->filepath);
    if (r != 0) {
         MAW_PERROR(tmpfile);
         goto end;
    }

end:
    (void)unlink(tmpfile);
    maw_free_context(ctx);
    return r;
}

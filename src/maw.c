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


static int maw_demux_cover(AVFormatContext **cover_fmt_ctx,
                           AVFormatContext *output_fmt_ctx,
                           const Metadata *metadata) {
    int r = INTERNAL_ERROR;
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
        r = UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }
    if ((*cover_fmt_ctx)->nb_streams > 1) {
        MAW_LOGF(MAW_ERROR, "%s: cover has more than one input stream\n", metadata->cover_path);
        r = UNSUPPORTED_INPUT_STREAMS;
        goto end;
    }
    codec_type = (*cover_fmt_ctx)->streams[0]->codecpar->codec_type;
    if (codec_type != AVMEDIA_TYPE_VIDEO) {
        MAW_LOGF(MAW_ERROR, "%s: cover does not contain a video stream (found %s)\n",
                 metadata->cover_path,
                 av_get_media_type_string(codec_type));
        r = UNSUPPORTED_INPUT_STREAMS;
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

static int maw_filter_crop_cover(AVFormatContext *input_fmt_ctx,
                                 AVFormatContext *output_fmt_ctx,
                                 const Metadata *metadata,
                                 int video_input_stream_index,
                                 AVCodecContext *dec_codec_ctx,
                                 AVFilterGraph **filter_graph,
                                 AVFilterContext **filter_buffersrc_ctx,
                                 AVFilterContext **filter_crop_ctx,
                                 AVFilterContext **filter_buffersink_ctx) {
    int r = INTERNAL_ERROR;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    const AVFilter *crop_filter = NULL;
    const AVFilter *buffersrc_filter  = NULL;
    const AVFilter *buffersink_filter = NULL;
    const char *crop_filter_args = NULL;
    char args[512];
    enum AVPixelFormat pix_fmts[2] = { AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE };

    if (video_input_stream_index == -1) {
        // The validity should already have been checked during demux
        goto end;
    }

    input_stream = input_fmt_ctx->streams[video_input_stream_index];

    *filter_graph = avfilter_graph_alloc();
    buffersrc_filter  = avfilter_get_by_name("buffer");
    buffersink_filter = avfilter_get_by_name("buffersink");
    crop_filter = avfilter_get_by_name("crop");

    if (filter_graph == NULL ||
        buffersrc_filter == NULL ||
        buffersink_filter == NULL ||
        crop_filter == NULL) {
        r = AVERROR(ENOMEM);
        MAW_AVERROR(r, "Failed to initialize crop filter");
        goto end;
    }

    // Buffer video source: the decoded frames from the decoder will be inserted here.
    r = snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 dec_codec_ctx->width, dec_codec_ctx->height, dec_codec_ctx->pix_fmt,
                 input_stream->time_base.num, input_stream->time_base.den,
                 dec_codec_ctx->sample_aspect_ratio.num, dec_codec_ctx->sample_aspect_ratio.den);

    if (r < 0 || r >= (int)sizeof(args)) {
        MAW_LOG(MAW_ERROR, "snprintf error/truncation");
        goto end;
    }

    r = avfilter_graph_create_filter(filter_buffersrc_ctx, buffersrc_filter, "in",
                                     args, NULL, *filter_graph);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to create input buffer filter");
        goto end;
    }
    MAW_LOGF(MAW_DEBUG, "Created input buffer filter: %s\n", args);

    r = avfilter_graph_create_filter(filter_buffersink_ctx, buffersink_filter, "out",
                                     NULL, NULL, *filter_graph);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to create output buffer filter");
        goto end;
    }
    MAW_LOG(MAW_DEBUG, "Created output buffer filter\n");

    crop_filter_args = "w=720:h=720:x=280:y=0"; // ,format=rgb24
    r = avfilter_graph_create_filter(filter_crop_ctx, crop_filter, "crop",
                                     crop_filter_args, NULL, *filter_graph);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to create crop filter");
        goto end;
    }
    MAW_LOG(MAW_DEBUG, "Created crop filter\n");

    r = avfilter_link(*filter_buffersrc_ctx, 0, *filter_crop_ctx, 0);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to link filters");
        goto end;
    }
    r = avfilter_link(*filter_crop_ctx, 0, *filter_buffersink_ctx, 0);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to link filters");
        goto end;
    }

    r = avfilter_graph_config(*filter_graph, NULL);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to configure filter graph");
        goto end;
    }

    // Always create a new video stream for the output
    output_stream = avformat_new_stream(output_fmt_ctx, NULL);

    r = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);
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

/**
 * @return 0 on success, negative AVERROR code on failure.
 */
static int maw_set_metadata(AVFormatContext *input_fmt_ctx,
                            AVFormatContext *output_fmt_ctx,
                            const Metadata *metadata) {
    int r = INTERNAL_ERROR;
    const AVDictionaryEntry *entry = NULL;

    if (metadata->clear_non_core_fields) {
        // Only keep some of the metadata
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
        // Keep the metadata as is
        r = av_dict_copy(&output_fmt_ctx->metadata, input_fmt_ctx->metadata, 0);
        if (r != 0) {
            goto end;
        }
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
                           const Metadata *metadata,
                           AVFormatContext **input_fmt_ctx,
                           AVFormatContext **output_fmt_ctx,
                           int *audio_input_stream_index,
                           int *video_input_stream_index) {
    int r = INTERNAL_ERROR;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    enum AVMediaType codec_type;
    bool is_attached_pic;

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
            MAW_LOGF(MAW_WARN, "%s: Audio input stream #%u (ignored)\n", input_filepath, i);
            continue;
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

    if (*audio_input_stream_index == -1) {
        r = UNSUPPORTED_INPUT_STREAMS;
        MAW_LOGF(MAW_ERROR, "%s: No audio streams\n", input_filepath);
        goto end;
    }

    for (unsigned int i = 0; i < (*input_fmt_ctx)->nb_streams; i++) {
        input_stream = (*input_fmt_ctx)->streams[i];
        codec_type = input_stream->codecpar->codec_type;
        is_attached_pic = codec_type == AVMEDIA_TYPE_VIDEO &&
                          input_stream->disposition == AV_DISPOSITION_ATTACHED_PIC;
        // Skip all streams except video streams with an attached_pic disposition
        if (!is_attached_pic || *video_input_stream_index != -1) {
            if ((int)i != *audio_input_stream_index)
                MAW_LOGF(MAW_WARN, "%s: Skipping %s input stream #%d\n",
                                    input_filepath, av_get_media_type_string(codec_type), i);
            continue;
        }

        *video_input_stream_index = i;


        // Do not demux the original video stream if it is not needed
        if (!NEEDS_ORIGINAL_COVER(metadata)) {
            continue;
        }

        output_stream = avformat_new_stream(*output_fmt_ctx, NULL);
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
        if (NEEDS_ORIGINAL_COVER(metadata)) {
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

    r = 0;
end:
    return r;
}

static int maw_mux(const char *input_filepath,
                   const char *output_filepath,
                   const Metadata *metadata,
                   AVFormatContext *input_fmt_ctx,
                   AVFormatContext *cover_fmt_ctx,
                   AVFormatContext *output_fmt_ctx,
                   int audio_input_stream_index,
                   int video_input_stream_index,
                   AVFilterContext *filter_buffersrc_ctx,
                   AVFilterContext *filter_buffersink_ctx,
                   AVCodecContext *dec_codec_ctx,
                   AVCodecContext *enc_codec_ctx) {
    int r = INTERNAL_ERROR;
    int prev_stream_index = -1;
    int output_stream_index = -1;
    AVStream *output_stream = NULL;
    AVStream *input_stream = NULL;
    // Packets contain compressed data from a stream, frames contain the
    // actual raw data. Filters can not be applied directly on packets, we
    // need to decode them into frames and re-encode them back into packets.
    AVPacket *pkt = NULL;
    AVPacket *filtered_pkt = NULL;
    AVFrame *frame = NULL;
    AVFrame *filtered_frame = NULL;
    FILE* fp;


    if (metadata->cover_policy == CROP_COVER) {
        // XXX
        output_fmt_ctx->streams[1]->codecpar->width = 720;
        output_fmt_ctx->streams[1]->codecpar->height = 720;
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

    pkt = av_packet_alloc();
    if (pkt == NULL) {
        MAW_LOGF(MAW_ERROR, "%s: Failed to allocate packet\n", output_filepath);
        goto end;
    }

    if (metadata->cover_policy == CROP_COVER) {
        frame = av_frame_alloc();
        filtered_frame = av_frame_alloc();
        filtered_pkt = av_packet_alloc();
        if (frame == NULL || filtered_frame == NULL || filtered_pkt == NULL) {
            r = AVERROR(ENOMEM);
            MAW_AVERROR(r, "Failed to initialize filter structures");
            goto end;
        }
    }

    // Mux streams from input file
    while (av_read_frame(input_fmt_ctx, pkt) == 0) {
        if (pkt->stream_index < 0 ||
            pkt->stream_index >= (int)input_fmt_ctx->nb_streams) {
            MAW_LOGF(MAW_ERROR, "%s: Invalid stream index: #%d\n", input_filepath,
                                                                   pkt->stream_index);
            goto end;
        }

        input_stream = input_fmt_ctx->streams[pkt->stream_index];

        if (pkt->stream_index == audio_input_stream_index) {
            // Audio stream
            output_stream_index = 0;
        }
        else if (pkt->stream_index == video_input_stream_index) {
            if (!NEEDS_ORIGINAL_COVER(metadata)) {
                // Skip original video stream
                continue;
            }
            // Video stream to keep
            output_stream_index = 1;
        }
        else {
            // Rate limit repeated log messages
            if (prev_stream_index != pkt->stream_index)
                MAW_LOGF(MAW_DEBUG, "%s: Ignoring packets from %s input stream #%d\n",
                         input_filepath,
                         av_get_media_type_string(input_stream->codecpar->codec_type),
                         pkt->stream_index);
            prev_stream_index = pkt->stream_index;
            continue;
        }

        output_stream = output_fmt_ctx->streams[output_stream_index];

        // The pkt will have the stream_index set to the stream index in the
        // input file. Remap it to the correct stream_index in the output file.
        pkt->stream_index = output_stream_index;

        if (pkt->stream_index == video_input_stream_index &&
            (metadata->cover_policy == CROP_COVER)) {

            // Send the packet to the decoder
            r = avcodec_send_packet(dec_codec_ctx, pkt);
            if (r != 0) {
                MAW_AVERROR(r, "Failed to send packet to decoder");
                goto end;
            }
            // Read the decoded frame
            r = avcodec_receive_frame(dec_codec_ctx, frame);
            if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
                break;
            }
            else if (r != 0) {
                MAW_AVERROR(r, "Failed to read decoded frame");
                goto end;
            }

            // Push the frame into the filter graph
            r = av_buffersrc_add_frame_flags(filter_buffersrc_ctx,
                                             frame,
                                             AV_BUFFERSRC_FLAG_KEEP_REF);
            if (r != 0) {
                MAW_AVERROR(r, "Error feeding the filtergraph");
                goto end;
            }
            av_frame_unref(frame);

            // Pull filtered frames from the filtergraph
            while (true) {
                r = av_buffersink_get_frame(filter_buffersink_ctx, filtered_frame);
                if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
                    break;
                }
                else if (r != 0) {
                    MAW_AVERROR(r, "Failed to read filtered frame");
                    goto end;
                }

                // Encode the frame into a packet
                r = avcodec_send_frame(enc_codec_ctx, filtered_frame);
                if (r != 0) {
                    MAW_AVERROR(r, "Error sending frame to encoder");
                    goto end;
                }
                // Read back the encoded packet
                r = avcodec_receive_packet(enc_codec_ctx, filtered_pkt);
                if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) {
                    break;
                }
                else if (r != 0) {
                    MAW_AVERROR(r, "Failed to read filtered packet");
                    goto end;
                }

                // Write the encoded packet to the output stream
                filtered_pkt->pos = -1;
                filtered_pkt->pts = AV_NOPTS_VALUE;
                filtered_pkt->duration = 0;
                filtered_pkt->stream_index = 1;

                // TODO DEBUG
                fp = fopen("test.png", "w");
                r = fwrite(filtered_pkt->data, filtered_pkt->size, 1, fp);
                r = fclose(fp);

                r = av_interleaved_write_frame(output_fmt_ctx, filtered_pkt);
                if (r != 0) {
                    MAW_AVERROR(r, "Failed to mux packet");
                    goto end;
                }
            }
        } else {
            if (pkt->stream_index == audio_input_stream_index) {
                av_packet_rescale_ts(pkt, input_stream->time_base,
                                          output_stream->time_base);
                pkt->pos = -1;
            }
            // The pkt passed to this function is automatically freed
            r = av_interleaved_write_frame(output_fmt_ctx, pkt);
            if (r != 0) {
                MAW_AVERROR(r, "Failed to mux packet");
                goto end;
            }
            // This warning: 'Encoder did not produce proper pts, making some up.'
            // appears for packets in cover art streams (since they do not have a
            // pts value set), its harmless, more info on pts:
            // http://dranger.com/ffmpeg/tutorial05.html
        }
    }

    // Mux streams from cover
    while (cover_fmt_ctx != NULL) {
        r = av_read_frame(cover_fmt_ctx, pkt);
        if (r != 0) {
            break; // No more frames
        }

        if (pkt->stream_index != 0) {
            r = INTERNAL_ERROR;
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
    av_packet_free(&filtered_pkt);
    av_frame_free(&frame);
    av_frame_free(&filtered_frame);
    return r;
}


// See "Stream copy" section of ffmpeg(1), that is what we are doing
// The output should always have either:
// 1 audio stream + 1 video stream
// 1 audio stream + 0 video streams
static int maw_remux(const char *input_filepath,
                     const char *output_filepath,
                     const Metadata *metadata) {
    int r;
    AVFormatContext *input_fmt_ctx = NULL;
    AVFormatContext *output_fmt_ctx = NULL;
    AVFormatContext *cover_fmt_ctx = NULL;
    AVFilterGraph *filter_graph = NULL;
    AVFilterContext *filter_buffersrc_ctx = NULL;
    AVFilterContext *filter_crop_ctx = NULL;
    AVFilterContext *filter_buffersink_ctx = NULL;
    int audio_input_stream_index = -1;
    int video_input_stream_index = -1;
    AVCodecContext *dec_codec_ctx = NULL;
    AVCodecContext *enc_codec_ctx = NULL;
    AVStream *stream = NULL;
    const AVCodec *dec_codec = NULL;
    const AVCodec *enc_codec = NULL;
    enum AVCodecID codec_id;

    r = maw_demux_media(input_filepath,
                        output_filepath,
                        metadata,
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
    else if (metadata->cover_policy == CROP_COVER) {
        codec_id = input_fmt_ctx->streams[video_input_stream_index]->codecpar->codec_id;
        dec_codec = avcodec_find_decoder(codec_id);
        if (dec_codec == NULL) {
            MAW_LOG(MAW_ERROR, "Failed to find decoder");
            goto end;
        }

        dec_codec_ctx = avcodec_alloc_context3(NULL);
        if (dec_codec_ctx == NULL) {
            r = AVERROR(ENOMEM);
            MAW_AVERROR(r, "Failed to allocate decoder context");
            goto end;
        }

        stream = input_fmt_ctx->streams[video_input_stream_index];
        r = avcodec_parameters_to_context(dec_codec_ctx, stream->codecpar);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to copy codec parameters");
            goto end;
        }

        r = avcodec_open2(dec_codec_ctx, dec_codec, NULL);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to open decoder context");
            goto end;
        }

        MAW_LOGF(MAW_DEBUG, "%s: Video stream #%d: video_size=%dx%d pix_fmt=%s pixel_aspect=%d/%d\n",
             input_filepath, video_input_stream_index,
             dec_codec_ctx->width, dec_codec_ctx->height,
             av_get_pix_fmt_name(dec_codec_ctx->pix_fmt),
             dec_codec_ctx->sample_aspect_ratio.num, dec_codec_ctx->sample_aspect_ratio.den);

        if (dec_codec_ctx->width == 720 && dec_codec_ctx->height == 720) {
            MAW_LOGF(MAW_DEBUG, "%s: Crop filter has already been applied\n", input_filepath);
        }
        else if (dec_codec_ctx->width != 1280 || dec_codec_ctx->height != 720) {
            MAW_LOGF(MAW_WARN, "%s: Crop filter not applied: unsupported cover dimensions: %dx%d\n",
                               input_filepath, dec_codec_ctx->width, dec_codec_ctx->height);

        }
        else {
            MAW_LOGF(MAW_DEBUG, "%s: Applying crop filter\n", input_filepath);

            r = maw_filter_crop_cover(input_fmt_ctx,
                                      output_fmt_ctx,
                                      metadata,
                                      video_input_stream_index,
                                      dec_codec_ctx,
                                      &filter_graph,
                                      &filter_buffersrc_ctx,
                                      &filter_crop_ctx,
                                      &filter_buffersink_ctx);
            if (r != 0)
                goto end;
        }

        // Create an encoder context, we need this to translate the output frames
        // from the filtergraph back into packets
        enc_codec = avcodec_find_encoder(codec_id);
        if (enc_codec == NULL) {
            MAW_LOG(MAW_ERROR, "Failed to find encoder");
            goto end;
        }

        enc_codec_ctx = avcodec_alloc_context3(enc_codec);
        if (enc_codec_ctx == NULL) {
            r = AVERROR(ENOMEM);
            MAW_AVERROR(r, "Failed to allocate encoder context");
            goto end;
        }

        enc_codec_ctx->time_base = (AVRational){1,1};
        enc_codec_ctx->framerate = input_fmt_ctx->streams[video_input_stream_index]->codecpar->framerate;
        enc_codec_ctx->max_b_frames = 1;
        // Use the same dimensions as the output stream
        enc_codec_ctx->width =  720; //output_fmt_ctx->streams[1]->codecpar->width;
        enc_codec_ctx->height = 720; //output_fmt_ctx->streams[1]->codecpar->height;
        // Use the same pix_fmt as the decoder
        enc_codec_ctx->pix_fmt = dec_codec_ctx->pix_fmt;

        r = avcodec_open2(enc_codec_ctx, enc_codec, NULL);
        if (r != 0) {
            MAW_AVERROR(r, "Failed to open encoder context");
            goto end;
        }
    }

    // The metadata for artist etc. is in the AVFormatContext, streams also have
    // a metadata field but these contain other stuff, e.g. audio streams can
    // have 'language' and 'handler_name'
    r = maw_set_metadata(input_fmt_ctx, output_fmt_ctx, metadata);
    if (r != 0) {
        MAW_AVERROR(r, "Failed to copy metadata");
        goto end;
    }

    r = maw_mux(input_filepath,
                output_filepath,
                metadata,
                input_fmt_ctx,
                cover_fmt_ctx,
                output_fmt_ctx,
                audio_input_stream_index,
                video_input_stream_index,
                filter_buffersrc_ctx,
                filter_buffersink_ctx,
                dec_codec_ctx,
                enc_codec_ctx);
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

    avcodec_free_context(&enc_codec_ctx);
    avcodec_free_context(&dec_codec_ctx);
    avfilter_graph_free(&filter_graph);
    return r;
}

int maw_update(const char *filepath,
               const Metadata *metadata) {
    int r = INTERNAL_ERROR;
    char tmpfile[] = "/tmp/maw.XXXXX.m4a";
    int tmphandle = mkstemps(tmpfile, sizeof(".m4a") - 1);

    if (tmphandle < 0) {
         MAW_PERROR(tmpfile);
         goto end;
    }
    (void)close(tmphandle);

    MAW_LOGF(MAW_DEBUG, "%s -> %s\n", filepath, tmpfile);

    r = maw_remux(filepath, tmpfile, metadata);
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

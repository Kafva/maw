#include "maw/tests/maw_verify.h"
#include "maw/av.h"
#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/utils.h"

bool maw_verify_file(const char *path, const char *expected_content) {
    char data[BUFSIZ];
    int read_bytes;
    bool ok = false;

    read_bytes = (int)readfile(path, data, sizeof data);
    if (read_bytes == 0) {
        goto end;
    }

    ok = memcmp(data, expected_content, strlen(expected_content)) == 0;
    if (!ok) {
        MAW_LOGF(MAW_ERROR, "Unexpected content in %s", path);
        goto end;
    }
end:
    return ok;
}

static bool maw_verify_cover(const AVFormatContext *fmt_ctx,
                             const MediaFile *mediafile) {
    int r;
    char cover_data[BUFSIZ];
    AVStream *stream = NULL;
    int read_bytes;
    bool ok = false;

    read_bytes = (int)readfile(mediafile->metadata->cover_path, cover_data,
                               sizeof cover_data);
    if (read_bytes == 0) {
        goto end;
    }

    if (fmt_ctx->nb_streams != 2) {
        MAW_LOGF(MAW_ERROR, "%s: Expected two streams: found %u",
                 mediafile->path, fmt_ctx->nb_streams);
        goto end;
    }

    stream = fmt_ctx->streams[1];
    if (stream->attached_pic.data == NULL) {
        MAW_LOGF(MAW_ERROR, "%s: video stream is empty", mediafile->path);
        goto end;
    }
    if (stream->attached_pic.size != read_bytes) {
        MAW_LOGF(MAW_ERROR, "%s: incorrect cover size: %d != %d",
                 mediafile->metadata->cover_path, stream->attached_pic.size,
                 read_bytes);
        goto end;
    }

    r = memcmp(stream->attached_pic.data, cover_data, (size_t)read_bytes);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "%s: cover data does not match",
                 mediafile->metadata->cover_path);
        goto end;
    }

    ok = true;
end:
    return ok;
}

bool maw_verify(const MediaFile *mediafile) {
    bool ok = false;
    bool is_unclean;
    int r;
    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *entry = NULL;

    if ((r = avformat_open_input(&fmt_ctx, mediafile->path, NULL, NULL))) {
        MAW_AVERROR(r, mediafile->path);
        goto end;
    }

    if ((r = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        MAW_AVERROR(r, mediafile->path);
        goto end;
    }

    is_unclean = false;

    // Verify metadata
    while ((entry = av_dict_iterate(fmt_ctx->metadata, entry))) {
        if (STR_EQ("title", entry->key)) {
            if (!LHS_EMPTY_OR_EQ(mediafile->metadata->title, entry->value))
                goto end;
        }
        else if (STR_EQ("artist", entry->key)) {
            if (!LHS_EMPTY_OR_EQ(mediafile->metadata->artist, entry->value))
                goto end;
        }
        else if (STR_EQ("album", entry->key)) {
            if (!LHS_EMPTY_OR_EQ(mediafile->metadata->album, entry->value))
                goto end;
        }
        else if (mediafile->metadata->clean &&
                 strcmp(entry->key, "major_brand") != 0 &&
                 strcmp(entry->key, "minor_version") != 0 &&
                 strcmp(entry->key, "compatible_brands") != 0 &&
                 strcmp(entry->key, "encoder") != 0) {
            MAW_LOGF(MAW_ERROR, "%s: Unexpected unclean field: %s",
                     mediafile->path, entry->key);
            goto end;
        }
        else if (!mediafile->metadata->clean && !is_unclean) {
            // Verify that genre was actually kept if clean is unset, we don't
            // verify that *all* other fields were kept
            is_unclean = strcmp(entry->key, "genre") == 0;
        }
    }

    if (!mediafile->metadata->clean && !is_unclean) {
        MAW_LOGF(MAW_ERROR, "%s: Expected unclean metadata", mediafile->path);
        goto end;
    }

    if (mediafile->metadata->cover_policy == COVER_CROP &&
        fmt_ctx->nb_streams == 2) {
        if (!(fmt_ctx->streams[VIDEO_OUTPUT_STREAM_INDEX]->codecpar->width ==
                  CROP_DESIRED_WIDTH &&
              fmt_ctx->streams[VIDEO_OUTPUT_STREAM_INDEX]->codecpar->height ==
                  CROP_ACCEPTED_HEIGHT)) {
            MAW_LOGF(
                MAW_ERROR, "%s: Expected cropped cover: found %dx%d",
                mediafile->path,
                fmt_ctx->streams[VIDEO_OUTPUT_STREAM_INDEX]->codecpar->width,
                fmt_ctx->streams[VIDEO_OUTPUT_STREAM_INDEX]->codecpar->height);
            goto end;
        }
    }
    else if (mediafile->metadata->cover_path != NULL) {
        // Configured cover should be present
        if (!maw_verify_cover(fmt_ctx, mediafile)) {
            goto end;
        }
    }
    else if (mediafile->metadata->cover_policy == COVER_CLEAR) {
        // No cover should be present
        if (fmt_ctx->nb_streams != 1) {
            MAW_LOGF(MAW_ERROR, "%s: Expected one stream: found %u",
                     mediafile->path, fmt_ctx->nb_streams);
            goto end;
        }
    }
    else {
        // Original cover should still be present (this could mean no cover)
        // we only check the stream count, we do not know what the original data
        // looked like
        if (fmt_ctx->nb_streams != 1 && fmt_ctx->nb_streams != 2) {
            MAW_LOGF(MAW_ERROR, "%s: Unexpected number of streams: found %u",
                     mediafile->path, fmt_ctx->nb_streams);
            goto end;
        }
    }

    ok = true;
end:
    avformat_close_input(&fmt_ctx);
    return ok;
}

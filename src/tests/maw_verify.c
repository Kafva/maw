#include "tests/maw_verify.h"
#include "log.h"
#include "utils.h"

bool maw_verify(const char *filepath,
                const Metadata *metadata) {
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
        else if (metadata->clear_non_core_fields &&
                 strcmp(entry->key, "major_brand") != 0 &&
                 strcmp(entry->key, "minor_version") != 0 &&
                 strcmp(entry->key, "compatible_brands") != 0 &&
                 strcmp(entry->key, "encoder") != 0) {
            // There should be no other fields
            goto end;
        }
    }

    if (metadata->cover_path != NULL) {
        // Configured cover should be present
        ok = maw_verify_cover(fmt_ctx, filepath, metadata);
    }
    else if (metadata->cover_policy == CLEAR_COVER) {
        // No cover should be present
        if (fmt_ctx->nb_streams != 1) {
            MAW_LOGF(MAW_ERROR, "%s: Expected one stream: found %u\n",
                     filepath, fmt_ctx->nb_streams);
            goto end;
        }
        ok = true;
    }
    else if (metadata->cover_policy == CROP_COVER && fmt_ctx->nb_streams == 2) {
        // TODO: Verify the dimensions of the cover (if one exists)
        ok = false;
    }
    else {
        // Original cover should still be present (this could mean no cover)
        // we only check the stream count, we do not know what the original data looked like
        if (fmt_ctx->nb_streams != 1 && fmt_ctx->nb_streams != 2) {
            MAW_LOGF(MAW_ERROR, "%s: Unexpected number of streams: found %u\n",
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
                      const Metadata *metadata) {
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


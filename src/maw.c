#include "maw.h"

#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>

int maw_init(int level) {
    av_log_set_level(level);
    return 0;
}

int maw_dump(const char *filepath) {
    int r;
    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *tag = NULL;

    if ((r = avformat_open_input(&fmt_ctx, filepath, NULL, NULL))) {
        fprintf(stderr, "Cannot open %s\n", filepath);
        return r;
    }

    if ((r = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Cannot find stream information\n");
        return r;
    }

    printf("=== %s\n", filepath);
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag))) {
        printf("%s=%.32s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);

    return 0;
}

int maw_update(const char *filepath, const struct Metadata *metadata) {
    (void)filepath;
    (void)metadata;
    return 0;
}

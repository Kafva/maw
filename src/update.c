#include "maw/update.h"
#include "maw/av.h"
#include "maw/log.h"
#include "maw/utils.h"

int maw_update(const MediaFile *mediafile) {
    int r = MAW_ERR_INTERNAL;
    char tmpfile[MAW_PATH_MAX] = "/tmp/maw.XXXXXX.";
    int tmphandle;
    MawAVContext *ctx = NULL;
    const char *ext;

    ext = extname(mediafile->path);

    if (ext == NULL || (!STR_EQ("mp4", ext) && !STR_EQ("m4a", ext))) {
        MAW_LOGF(MAW_WARN, "%s: Skipping unsupported format", mediafile->path);
        r = 0;
        goto end;
    }

    MAW_STRLCAT(tmpfile, ext);
    tmphandle = mkstemps(tmpfile, (int)strlen(ext));

    if (mediafile->metadata == NULL) {
        MAW_LOGF(MAW_ERROR, "%s: Invalid metadata configuration",
                 mediafile->path);
        goto end;
    }

    if (tmphandle < 0) {
        MAW_PERROR(tmpfile);
        goto end;
    }
    (void)close(tmphandle);

    MAW_LOGF(MAW_DEBUG, "%s -> %s", mediafile->path, tmpfile);

    ctx = maw_av_init_context(mediafile, tmpfile);
    if (ctx == NULL)
        goto end;

    r = maw_av_remux(ctx);
    if (r != 0) {
        goto end;
    }

    if (on_same_device(tmpfile, mediafile->path)) {
        r = rename(tmpfile, mediafile->path);
        if (r != 0) {
            MAW_PERROR(tmpfile);
            goto end;
        }
    }
    else {
        r = movefile(tmpfile, mediafile->path);
        if (r != 0)
            goto end;
    }

    r = 0;
end:
    (void)unlink(tmpfile);
    maw_av_free_context(ctx);
    return r;
}

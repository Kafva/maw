#include "maw/maw.h"
#include "maw/av.h"
#include "maw/log.h"
#include "maw/utils.h"

int maw_update(const MediaFile *mediafile) {
    int r = MAW_ERR_INTERNAL;
    char tmpfile[] = "/tmp/maw.XXXXXX.m4a";
    int tmphandle = mkstemps(tmpfile, sizeof(".m4a") - 1);
    MawAVContext *ctx = NULL;

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

// Create a hidden .m3u playlist under the music_dir for each entry under
// `playlists`.
int maw_gen_playlists(MawConfig *cfg) {
    int r = MAW_ERR_INTERNAL;
    PlaylistEntry *p = NULL;
    PlaylistPath *pp = NULL;

    TAILQ_FOREACH(p, &(cfg->playlists_head), entry) {
        TAILQ_FOREACH(pp, &(p->value.playlist_paths_head), entry) {}
    }

    r = 0;
end:
    return r;
}

void maw_mediafiles_free(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count) {
    for (ssize_t i = 0; i < count; i++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        free((void *)mediafiles[i].path);
#pragma GCC diagnostic pop
    }
}

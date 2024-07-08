#include "maw/maw.h"
#include "maw/av.h"
#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/utils.h"

#include <dirent.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

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

// Return zero if the directory entry should be excluded
static int select_files(const struct dirent *entry) {
    return (entry->d_type == DT_REG && entry->d_name[0] != '.');
}

// Create a hidden .m3u playlist under the music_dir for each entry under
// `playlists`.
int maw_gen_playlists(MawConfig *cfg) {
    int r = MAW_ERR_INTERNAL;
    PlaylistEntry *p = NULL;
    PlaylistPath *pp = NULL;
    char playlistfile[MAW_CFG_PATH_MAX];
    char path[MAW_CFG_PATH_MAX];
    int fd = -1;
    DIR *dir = NULL;
    struct dirent **namelist = NULL;
    int names_count = -1;
    size_t pathsize;
    struct stat s;

    TAILQ_FOREACH(p, &(cfg->playlists_head), entry) {
        r = maw_playlist_path(cfg, p->value.name, playlistfile,
                              sizeof(playlistfile));
        if (r != 0) {
            goto end;
        }

        fd = open(playlistfile, O_CREAT | O_WRONLY, 0644);
        if (fd <= 0) {
            MAW_PERRORF("open", playlistfile);
            goto end;
        }

        TAILQ_FOREACH(pp, &(p->value.playlist_paths_head), entry) {

            MAW_STRLCPY(path, cfg->music_dir);
            MAW_STRLCAT(path, "/");
            MAW_STRLCAT(path, pp->path);

            r = stat(path, &s);
            if (r != 0) {
                MAW_PERRORF("stat", path);
                goto end;
            }

            if (S_ISREG(s.st_mode)) {
                pathsize = strlen(pp->path);
                MAW_WRITE(fd, pp->path, pathsize);
                MAW_WRITE(fd, "\n", 1);
            }
            else if (S_ISDIR(s.st_mode)) {
                if ((dir = opendir(path)) == NULL) {
                    MAW_PERRORF("opendir", path);
                    goto end;
                }

                // Scan the directory contents alphabetically
                names_count = scandir(path, &namelist, select_files, alphasort);
                if (names_count <= 0) {
                    MAW_PERRORF("scandir", path);
                    goto end;
                }

                for (int i = 0; i < names_count; i++) {
                    // XXX: Overwrite the full-path of the playlist entry
                    // with the path to the current entry
                    MAW_STRLCPY(path, pp->path);
                    MAW_STRLCAT(path, "/");
                    MAW_STRLCAT(path, namelist[i]->d_name);

                    pathsize = strlen(path);
                    MAW_WRITE(fd, path, pathsize);
                    MAW_WRITE(fd, "\n", 1);

                    free(namelist[i]);
                    namelist[i] = NULL;
                }
                free(namelist);
                namelist = NULL;

                (void)closedir(dir);
                dir = NULL;
            }
        }

        MAW_LOGF(MAW_DEBUG, "Generated: %s", playlistfile);
        (void)close(fd);
    }

    r = 0;
end:
    if (namelist != NULL) {
        for (int i = 0; i < names_count; i++) {
            if (namelist[i] != NULL) {
                free(namelist[i]);
            }
        }
        free(namelist);
    }
    if (dir != NULL)
        (void)closedir(dir);
    if (fd != -1)
        (void)close(fd);
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

#include "maw/playlists.h"
#include "maw/cfg.h"
#include "maw/log.h"

#include <dirent.h>
#include <glob.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Return zero if the directory entry should be excluded
static int select_files(const struct dirent *entry) {
    return (entry->d_type == DT_REG && entry->d_name[0] != '.');
}

int maw_playlists_path(MawConfig *cfg, const char *name, char *out,
                       size_t size) {
    int r = MAW_ERR_INTERNAL;
    MAW_STRLCPY_SIZE(out, cfg->music_dir, size);
    MAW_STRLCAT_SIZE(out, "/.", size);
    MAW_STRLCAT_SIZE(out, name, size);
    MAW_STRLCAT_SIZE(out, ".m3u", size);
    r = 0;
end:
    return r;
}

// Create a hidden .m3u playlist under the music_dir for each entry under
// `playlists`.
int maw_playlists_gen(MawConfig *cfg) {
    int r = MAW_ERR_INTERNAL;
    PlaylistEntry *p = NULL;
    PlaylistPath *pp = NULL;
    char playlistfile[MAW_PATH_MAX];
    char path[MAW_PATH_MAX];
    int fd = -1;
    DIR *dir = NULL;
    struct dirent **namelist = NULL;
    int names_count = -1;
    size_t pathsize;
    size_t linecnt;
    size_t music_dir_pathlen;
    struct stat s;
    char *glob_path;
    glob_t glob_result;

    music_dir_pathlen = strlen(cfg->music_dir) + 1;

    TAILQ_FOREACH(p, &(cfg->playlists_head), entry) {
        r = maw_playlists_path(cfg, p->value.name, playlistfile,
                               sizeof(playlistfile));
        if (r != 0) {
            goto end;
        }

        fd = open(playlistfile, O_CREAT | O_WRONLY, 0644);
        if (fd <= 0) {
            MAW_PERRORF("open", playlistfile);
            goto end;
        }

        linecnt = 0;
        TAILQ_FOREACH(pp, &(p->value.playlist_paths_head), entry) {

            MAW_STRLCPY(path, cfg->music_dir);
            MAW_STRLCAT(path, "/");
            MAW_STRLCAT(path, pp->path);

            if (strchr(path, '*') != NULL) {
                r = glob(path, GLOB_TILDE, NULL, &glob_result);
                if (r != 0) {
                    MAW_PERRORF("glob", path);
                    goto end;
                }

                for (size_t i = 0; i < glob_result.gl_pathc; i++) {
                    glob_path = glob_result.gl_pathv[i] + music_dir_pathlen;
                    pathsize = strlen(glob_path);
                    MAW_WRITE(fd, glob_path, pathsize);
                    MAW_WRITE(fd, "\n", 1);
                    linecnt++;
                }
                globfree(&glob_result);
            }
            else {
                r = stat(path, &s);
                if (r != 0) {
                    MAW_PERRORF("stat", path);
                    goto end;
                }

                if (S_ISREG(s.st_mode)) {
                    pathsize = strlen(pp->path);
                    MAW_WRITE(fd, pp->path, pathsize);
                    MAW_WRITE(fd, "\n", 1);
                    linecnt++;
                }
                else if (S_ISDIR(s.st_mode)) {
                    if ((dir = opendir(path)) == NULL) {
                        MAW_PERRORF("opendir", path);
                        goto end;
                    }

                    // Scan the directory contents alphabetically
                    names_count =
                        scandir(path, &namelist, select_files, alphasort);
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
                        linecnt++;

                        free(namelist[i]);
                        namelist[i] = NULL;
                    }
                    free(namelist);
                    namelist = NULL;

                    (void)closedir(dir);
                    dir = NULL;
                }
            }
        }

        MAW_LOGF(MAW_INFO, "Generated: %-38s [%zu item(s)]", playlistfile,
                 linecnt);
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

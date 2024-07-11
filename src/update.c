#include "maw/update.h"
#include "maw/av.h"
#include "maw/cfg.h"
#include "maw/log.h"
#include "maw/maw.h"
#include "maw/utils.h"

#include <dirent.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void maw_update_merge_metadata(const Metadata *original, Metadata *new);
static bool maw_update_add(const char *filepath, Metadata *metadata,
                           MediaFile mediafiles[MAW_MAX_FILES],
                           ssize_t *mediafiles_count);
static bool maw_update_should_alloc(MawArguments *args,
                                    MetadataEntry *metadata_entry);

////////////////////////////////////////////////////////////////////////////////

// Merge the metadata from `original` into `new`.
// If a metadata field is set in `original` AND unset it `new`, use the
// `original` value, otherwise keep the new value.
static void maw_update_merge_metadata(const Metadata *original, Metadata *new) {
    if (original->title != NULL && new->title == NULL) {
        new->title = strdup(original->title);
    }
    if (original->album != NULL && new->album == NULL) {
        new->album = strdup(original->album);
    }
    if (original->artist != NULL && new->artist == NULL) {
        new->artist = strdup(original->artist);
    }
    // Reuse the original value if the `new` item does not specify anything
    if (original->cover_policy != COVER_POLICY_UNSPECIFIED &&
        new->cover_policy == COVER_POLICY_UNSPECIFIED) {
        new->cover_policy = original->cover_policy;
        if (new->cover_policy == COVER_POLICY_PATH) {
            new->cover_path = strdup(original->cover_path);
        }
    }
}

static bool maw_update_add(const char *filepath, Metadata *metadata,
                           MediaFile mediafiles[MAW_MAX_FILES],
                           ssize_t *mediafiles_count) {
    MediaFile *latest;
    uint32_t digest;

    if (*mediafiles_count > MAW_MAX_FILES) {
        MAW_LOGF(MAW_ERROR, "Cannot process more than %d file(s)",
                 MAW_MAX_FILES);
        return false;
    }

    digest = hash(filepath);

    for (ssize_t i = 0; i < *mediafiles_count; i++) {
        if (mediafiles[i].path_digest == digest) {
            maw_update_merge_metadata(mediafiles[i].metadata, metadata);
            mediafiles[i].metadata = metadata;
            MAW_LOGF(MAW_DEBUG, "Replaced: %s", mediafiles[i].path);
            return true;
        }
    }

    *mediafiles_count += 1;
    latest = &mediafiles[*mediafiles_count - 1];
    latest->path = strdup(filepath);
    latest->path_digest = hash(filepath);
    latest->metadata = metadata;
    MAW_LOGF(MAW_DEBUG, "Added: %s", latest->path);

    return true;
}

// If paths were provided on the command line, only add the sections
// that have a matching prefix
static bool maw_update_should_alloc(MawArguments *args,
                                    MetadataEntry *metadata_entry) {
    size_t patlen;
    size_t arglen;

    // Include all entries by default
    if (args->cmd_args_count == 0) {
        return true;
    }

    patlen = strlen(metadata_entry->pattern);

    for (int i = 0; i < args->cmd_args_count; i++) {
        // The exact path provided on the command line
        if (STR_EQ(metadata_entry->pattern, args->cmd_args[i]))
            return true;

        // A path *beneath* the path provided on the command line
        if (STR_HAS_PREFIX(metadata_entry->pattern, args->cmd_args[i])) {
            arglen = strlen(args->cmd_args[i]);
            if (patlen > arglen && metadata_entry->pattern[arglen] == '/')
                return true;
        }
    }

    return false;
}

// Given our *cfg, create a MediaFile[] that we can feed to the job launcher.
// Later matches in the config file will take precedence!
int maw_update_load(MawConfig *cfg, MawArguments *args,
                    MediaFile mediafiles[MAW_MAX_FILES],
                    ssize_t *mediafiles_count) {
    int r = MAW_ERR_INTERNAL;
    MetadataEntry *metadata_entry = NULL;
    DIR *dir = NULL;
    struct dirent *entry;
    struct stat s;
    glob_t glob_result;
    char complete_pattern[MAW_PATH_MAX];
    char filepath[MAW_PATH_MAX];
    size_t music_dir_idx;
    bool ok;

    MAW_STRLCPY(complete_pattern, cfg->music_dir);
    MAW_STRLCAT(complete_pattern, "/");
    music_dir_idx = strlen(complete_pattern);

    TAILQ_FOREACH(metadata_entry, &(cfg->metadata_head), entry) {
        ok = maw_update_should_alloc(args, metadata_entry);
        if (!ok) {
            MAW_LOGF(MAW_DEBUG, "Skipping: %s", metadata_entry->pattern);
            continue;
        }

        // Keep the leading path across iterations and append the new pattern
        complete_pattern[music_dir_idx] = '\0';
        MAW_STRLCAT(complete_pattern, metadata_entry->pattern);

        if (strchr(complete_pattern, '*') != NULL) {
            r = glob(complete_pattern, GLOB_TILDE, NULL, &glob_result);
            if (r != 0) {
                MAW_PERRORF("glob", complete_pattern);
                goto end;
            }

            for (size_t i = 0; i < glob_result.gl_pathc; i++) {
                if (!maw_update_add(glob_result.gl_pathv[i],
                                    &metadata_entry->value, mediafiles,
                                    mediafiles_count))
                    goto end;
            }
            globfree(&glob_result);
        }
        else {
            r = stat(complete_pattern, &s);
            if (r != 0) {
                MAW_PERRORF("stat", complete_pattern);
                goto end;
            }

            if (S_ISREG(s.st_mode)) {
                if (!maw_update_add(complete_pattern, &metadata_entry->value,
                                    mediafiles, mediafiles_count))
                    goto end;
            }
            else if (S_ISDIR(s.st_mode)) {
                if ((dir = opendir(complete_pattern)) == NULL) {
                    MAW_PERRORF("opendir", complete_pattern);
                    goto end;
                }

                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_type != DT_REG) {
                        continue;
                    }
                    if (entry->d_name[0] == '.') {
                        continue;
                    }
                    MAW_STRLCPY(filepath, complete_pattern);
                    MAW_STRLCAT(filepath, "/");
                    MAW_STRLCAT(filepath, entry->d_name);
                    if (!maw_update_add(filepath, &metadata_entry->value,
                                        mediafiles, mediafiles_count)) {
                        goto end;
                    }
                }
                (void)closedir(dir);
                dir = NULL;
            }
        }
    }

    r = 0;
end:
    if (dir != NULL)
        (void)closedir(dir);
    return r;
}

void maw_update_dump(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count) {
    printf("{\n");
    for (ssize_t i = 0; i < count; i++) {
        printf("  \"%s\": {\n", mediafiles[i].path);
        printf("    \"" MAW_CFG_KEY_TITLE "\": \"%s\",\n",
               mediafiles[i].metadata->title);
        printf("    \"" MAW_CFG_KEY_ALBUM "\": \"%s\",\n",
               mediafiles[i].metadata->album);
        printf("    \"" MAW_CFG_KEY_ARTIST "\": \"%s\",\n",
               mediafiles[i].metadata->artist);
        printf("    \"" MAW_CFG_KEY_COVER "\": \"%s\",\n",
               MAW_COVER_TOSTR(mediafiles[i].metadata));
        printf(
            "    \"" MAW_CFG_KEY_CLEAN "\": \"%s\"\n",
            maw_cfg_clean_policy_tostr(mediafiles[i].metadata->clean_policy));
        printf(i == count - 1 ? "  }\n" : "  },\n");
    }
    printf("}\n");
}

void maw_update_free(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count) {
    for (ssize_t i = 0; i < count; i++) {
        free((void *)mediafiles[i].path);
    }
}

int maw_update(const MediaFile *mediafile, bool dry_run) {
    int r = MAW_ERR_INTERNAL;
    char tmpfile[MAW_PATH_MAX];
    char *tmpdir;
    int tmphandle;
    MawAVContext *ctx = NULL;
    const char *ext;

    tmpfile[0] = '\0';

    // Check argument sanity
    if (mediafile == NULL || mediafile->metadata == NULL ||
        mediafile->path == NULL) {
        MAW_LOG(MAW_ERROR, "No metadata configuration provided");
        goto end;
    }
    if (mediafile->metadata->cover_policy != COVER_POLICY_PATH &&
        mediafile->metadata->cover_path != NULL) {
        MAW_LOGF(MAW_ERROR,
                 "%s: cover_path should be unset for current policy: %s",
                 mediafile->path,
                 maw_cfg_cover_policy_tostr(mediafile->metadata->cover_policy));
        goto end;
    }
    if (mediafile->metadata->cover_policy == COVER_POLICY_PATH &&
        mediafile->metadata->cover_path == NULL) {
        MAW_LOGF(MAW_ERROR,
                 "%s: cover_path should be set for current policy: %s",
                 mediafile->path,
                 maw_cfg_cover_policy_tostr(mediafile->metadata->cover_policy));
        goto end;
    }

    ext = extname(mediafile->path);

    if (ext == NULL || (!STR_EQ("mp4", ext) && !STR_EQ("m4a", ext))) {
        MAW_LOGF(MAW_WARN, "%s: Skipping unsupported format", mediafile->path);
        r = 0;
        goto end;
    }

    // Define temp location for output file under IMPAIR, this allows
    // for easy overrides to speed up execution if /tmp is on another device.
    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL)
        tmpdir = "/tmp";

    MAW_STRLCPY(tmpfile, tmpdir);
    MAW_STRLCAT(tmpfile, "/maw.XXXXXX.");
    MAW_STRLCAT(tmpfile, ext);

    // +1 for the last '.'
    tmphandle = mkstemps(tmpfile, (int)strlen(ext) + 1);

    if (tmphandle < 0) {
        MAW_PERRORF("mkstemps", tmpfile);
        goto end;
    }
    (void)close(tmphandle);

    MAW_LOGF(MAW_DEBUG, "%s -> %s", mediafile->path, tmpfile);

    // Initialize libav contexts
    ctx = maw_av_init_context(mediafile, tmpfile);
    if (ctx == NULL)
        goto end;

    // Remux the input file
    r = maw_av_remux(ctx);
    if (r != 0) {
        goto end;
    }

    if (!dry_run) {
        // Replace the input file with the output file
        if (on_same_device(tmpfile, mediafile->path)) {
            r = rename(tmpfile, mediafile->path);
            if (r != 0) {
                MAW_PERRORF("rename", tmpfile);
                goto end;
            }
        }
        else {
            r = movefile(tmpfile, mediafile->path);
            if (r != 0)
                goto end;
        }
    }

    r = 0;
end:
    if (tmpfile[0] != '\0')
        (void)unlink(tmpfile);
    maw_av_free_context(ctx);
    return r;
}

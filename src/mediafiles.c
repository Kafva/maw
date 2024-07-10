#include <dirent.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "maw/log.h"
#include "maw/mediafiles.h"
#include "maw/utils.h"

static void maw_mediafiles_merge_metadata(const Metadata *original,
                                          Metadata *new);
static bool maw_mediafiles_add(const char *filepath, Metadata *metadata,
                               MediaFile mediafiles[MAW_MAX_FILES],
                               ssize_t *mediafiles_count);
static bool maw_mediafiles_should_alloc(MawArguments *args,
                                        MetadataEntry *metadata_entry);

////////////////////////////////////////////////////////////////////////////////

// Update the `new` metadata with fields from the `original` if applicable
static void maw_mediafiles_merge_metadata(const Metadata *original,
                                          Metadata *new) {
    if (original->title != NULL && new->title == NULL) {
        new->title = strdup(original->title);
    }
    if (original->album != NULL && new->album == NULL) {
        new->album = strdup(original->album);
    }
    if (original->artist != NULL && new->artist == NULL) {
        new->artist = strdup(original->artist);
    }
    if (original->cover_path != NULL && new->cover_path == NULL) {
        new->cover_path = strdup(original->cover_path);
    }
    if (original->cover_policy != COVER_UNSPECIFIED &&
        new->cover_policy == COVER_UNSPECIFIED) {
        new->cover_policy = original->cover_policy;
    }

    // Always keep the new value for `clean`
}

static bool maw_mediafiles_add(const char *filepath, Metadata *metadata,
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
            maw_mediafiles_merge_metadata(mediafiles[i].metadata, metadata);
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
static bool maw_mediafiles_should_alloc(MawArguments *args,
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
int maw_mediafiles_alloc(MawConfig *cfg, MawArguments *args,
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
        ok = maw_mediafiles_should_alloc(args, metadata_entry);
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
                if (!maw_mediafiles_add(glob_result.gl_pathv[i],
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
                if (!maw_mediafiles_add(complete_pattern,
                                        &metadata_entry->value, mediafiles,
                                        mediafiles_count))
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
                    if (!maw_mediafiles_add(filepath, &metadata_entry->value,
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

void maw_mediafiles_free(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count) {
    for (ssize_t i = 0; i < count; i++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        free((void *)mediafiles[i].path);
#pragma GCC diagnostic pop
    }
}

#ifndef CFG_H
#define CFG_H

#include "maw/maw.h"
#include <yaml.h>

#define MAW_CFG_KEY_ART          "art_dir"
#define MAW_CFG_KEY_MUSIC        "music_dir"
#define MAW_CFG_KEY_PLAYLISTS    "playlists"
#define MAW_CFG_KEY_METADATA     "metadata"
#define MAW_CFG_KEY_TITLE        "title"
#define MAW_CFG_KEY_ALBUM        "album"
#define MAW_CFG_KEY_ARTIST       "artist"
#define MAW_CFG_KEY_COVER        "cover"
#define MAW_CFG_KEY_COVER_POLICY "cover_policy"
#define MAW_CFG_KEY_CLEAN        "clean"

#define MAW_CFG_MAX_DEPTH 3

#define MAW_CFG_PATH_MAX 1024

enum YamlKey {
    KEY_NONE,
    KEY_INVALID,
    KEY_ARBITRARY,
    KEY_ART,
    KEY_MUSIC,
    KEY_PLAYLISTS,
    KEY_METADATA,
    KEY_ALBUM,
    KEY_ARTIST,
    KEY_COVER,
    KEY_COVER_POLICY,
    KEY_CLEAN,
};

struct YamlContext {
    const char *filepath;
    yaml_token_type_t next_token_type;
    enum YamlKey keypath[MAW_CFG_MAX_DEPTH];
    ssize_t key_count;
} typedef YamlContext;

void maw_cfg_dump(MawConfig *cfg);
void maw_cfg_free(MawConfig *cfg);
int maw_cfg_parse(const char *filepath, MawConfig **cfg)
    __attribute__((warn_unused_result));
int maw_cfg_mediafiles_alloc(MawConfig *cfg,
                             MediaFile mediafiles[MAW_MAX_FILES],
                             ssize_t *mediafiles_count)
    __attribute__((warn_unused_result));

void maw_cfg_mediafiles_free(MediaFile mediafiles[MAW_MAX_FILES],
                             ssize_t count);

#endif // CFG_H

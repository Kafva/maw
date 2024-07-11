#ifndef MAW_CFG_H
#define MAW_CFG_H

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

const char *maw_cfg_cover_policy_tostr(CoverPolicy key);
void maw_cfg_dump(MawConfig *cfg);
void maw_cfg_free(MawConfig *cfg);
int maw_cfg_parse(const char *filepath, MawConfig **cfg)
    __attribute__((warn_unused_result));

#endif // MAW_CFG_H

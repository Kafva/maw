#ifndef CFG_H
#define CFG_H

#include <sys/queue.h>
#include <yaml.h>
#include "maw/maw.h"

#define MAW_CFG_KEY_ART             "art_dir"
#define MAW_CFG_KEY_MUSIC           "music_dir"
#define MAW_CFG_KEY_PLAYLISTS       "playlists"
#define MAW_CFG_KEY_METADATA        "metadata"
#define MAW_CFG_KEY_ALBUM           "album"
#define MAW_CFG_KEY_ARTIST          "artist"
#define MAW_CFG_KEY_COVER           "cover"
#define MAW_CFG_KEY_COVER_POLICY    "cover_policy"
#define MAW_CFG_KEY_CLEAN           "clean"

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

struct PlaylistPath {
    const char *path;
    SLIST_ENTRY(PlaylistPath) entry;
} typedef PlaylistPath;

struct Playlist {
    const char *name;
    SLIST_HEAD(,PlaylistPath) playlist_paths_head;
} typedef Playlist;

struct PlaylistEntry {
    Playlist value;
    SLIST_ENTRY(PlaylistEntry) entry;
} typedef PlaylistEntry;

struct MetadataEntry {
    Metadata value;
    SLIST_ENTRY(MetadataEntry) entry;
} typedef MetadataEntry;

struct MawConfig {
    char *art_dir;
    char *music_dir;
    SLIST_HEAD(, PlaylistEntry) playlists_head;
    SLIST_HEAD(, MetadataEntry) metadata_head;
} typedef MawConfig;

struct YamlContext {
    const char *filepath;
    int next_token_type;
    enum YamlKey keypath[MAW_CFG_MAX_DEPTH];
    size_t key_count;

} typedef YamlContext;


void maw_cfg_dump(MawConfig *cfg);
int maw_cfg_parse(const char *filepath, MawConfig **cfg) __attribute__((warn_unused_result));

#endif // CFG_H

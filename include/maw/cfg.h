#ifndef CFG_H
#define CFG_H

// TAILQ is used instead of STAILQ to make the code more portable.
// STAILQ_LAST is only provided by <bsd/sys/queue.h> and generates
// -Wgnu-statement-expression-from-macro-expansion warnings on Linux,
// TAILQ_LAST works fine on both Linux and BSD.

#include <sys/queue.h>
#include <yaml.h>
#include "maw/maw.h"

#define MAW_CFG_KEY_ART             "art_dir"
#define MAW_CFG_KEY_MUSIC           "music_dir"
#define MAW_CFG_KEY_PLAYLISTS       "playlists"
#define MAW_CFG_KEY_METADATA        "metadata"
#define MAW_CFG_KEY_TITLE           "title"
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
    TAILQ_ENTRY(PlaylistPath) entry;
} typedef PlaylistPath;

struct Playlist {
    const char *name;
    TAILQ_HEAD(,PlaylistPath) playlist_paths_head;
} typedef Playlist;

struct PlaylistEntry {
    Playlist value;
    TAILQ_ENTRY(PlaylistEntry) entry;
} typedef PlaylistEntry;

struct MetadataEntry {
    const char *pattern;
    Metadata value;
    TAILQ_ENTRY(MetadataEntry) entry;
} typedef MetadataEntry;

struct MawConfig {
    char *art_dir;
    char *music_dir;
    TAILQ_HEAD(PlaylistEntryHead, PlaylistEntry) playlists_head;
    TAILQ_HEAD(MetadataEntryHead, MetadataEntry) metadata_head;
} typedef MawConfig;

struct YamlContext {
    const char *filepath;
    yaml_token_type_t next_token_type;
    enum YamlKey keypath[MAW_CFG_MAX_DEPTH];
    ssize_t key_count;
} typedef YamlContext;


void maw_cfg_dump(MawConfig *cfg);
void maw_cfg_free(MawConfig *cfg);
int maw_cfg_parse(const char *filepath, MawConfig **cfg) __attribute__((warn_unused_result));
int maw_cfg_alloc_mediafiles(MawConfig *cfg,
                             MediaFile mediafiles[MAW_MAX_FILES],
                             ssize_t *mediafiles_count) __attribute__((warn_unused_result));

#endif // CFG_H

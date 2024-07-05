#ifndef CFG_H
#define CFG_H

#include <yaml.h>
#include "maw/maw.h"

#define MAW_CFG_ART_KEY         "art_dir"
#define MAW_CFG_MUSIC_KEY       "music_dir"
#define MAW_CFG_PLAYLISTS_KEY   "playlists"
#define MAW_CFG_METADATA_KEY    "metadata"

struct PlaylistEntry {
    Playlist value;
    struct PlaylistEntry *next;
} typedef PlaylistEntry;

struct MetadataEntry {
    Metadata value;
    struct MetadataEntry *next;
} typedef MetadataEntry;

struct MawConfig {
    char *art_dir;
    char *music_dir;
    PlaylistEntry *playlists; // LINKED LIST
    size_t playlist_count;
    MetadataEntry *metadata; // LINKED_LIST
    size_t metadata_count;
} typedef MawConfig;

enum MawConfigSection {
    MAW_CFG_SECTION_TOP,
    MAW_CFG_SECTION_ART_DIR,
    MAW_CFG_SECTION_MUSIC_DIR,
    MAW_CFG_SECTION_PLAYLISTS,
    MAW_CFG_SECTION_PLAYLISTS_ENTRY,
    MAW_CFG_SECTION_METADATA,
    MAW_CFG_SECTION_METADATA_ENTRY,
} typedef MawConfigSection;


int maw_cfg_yaml_parse(const char *filepath, MawConfig **cfg) __attribute__((warn_unused_result));

#endif // CFG_H

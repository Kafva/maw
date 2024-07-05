#ifndef CFG_H
#define CFG_H

#include <sys/queue.h>
#include <yaml.h>
#include "maw/maw.h"

#define MAW_CFG_ART_KEY         "art_dir"
#define MAW_CFG_MUSIC_KEY       "music_dir"
#define MAW_CFG_PLAYLISTS_KEY   "playlists"
#define MAW_CFG_METADATA_KEY    "metadata"


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
void maw_cfg_dump(MawConfig *cfg);

#endif // CFG_H
